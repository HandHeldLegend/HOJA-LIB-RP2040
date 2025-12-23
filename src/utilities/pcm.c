#include "utilities/pcm.h"
#include "hal/mutex_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "switch/switch_haptics.h"
#include "utilities/pcm_samples.h"
#include "utilities/settings.h"
#include "utilities/crosscore_snapshot.h"

#include "hoja_bsp.h"
#if HOJA_BSP_CHIPSET == CHIPSET_RP2040
// Special float functions for RP2040
#include "pico/float.h"
#endif

int16_t _pcm_sine_table[PCM_SINE_TABLE_SIZE];

// Minimum amplitude scalers for low and high frequencies
volatile uint32_t _lo_amp_scaler_fixed_min = 0;
volatile uint32_t _hi_amp_scaler_fixed_min  = 0;

// Scalers for low and high frequencies
volatile uint32_t _lo_amp_scaler_fixed = (uint32_t) (0.5f * PCM_AMPLITUDE_SHIFT_FIXED);
volatile uint32_t _hi_amp_scaler_fixed = (uint32_t) (0.5f * PCM_AMPLITUDE_SHIFT_FIXED);

volatile uint32_t _pwm_wrap_max = PCM_MAX_SAFE_RATIO * PCM_WRAP_HALF_VAL; // Max PWM wrap value

volatile uint32_t _external_sample_scaler = 0;

volatile float _pcm_param_min_lo = PCM_LO_FREQUENCY_MIN; // Minimum low frequency parameter
volatile float _pcm_param_min_hi = PCM_HI_FREQUENCY_MIN; // Minimum high frequency parameter

#define TWO_PI 2.0f * M_PI

// DC offset ramp rate per sample (higher = faster attack/decay)
// At 8000 Hz sample rate: rate of 1 = 256ms, rate of 8 = 32ms
#define DC_OFFSET_RAMP_RATE 8

void pcm_debug_adjust_param(uint8_t param_type, float amount)
{
    switch(param_type)
    {
        case PCM_DEBUG_PARAM_MIN_HI:
            _pcm_param_min_hi += amount;
            _pcm_param_min_hi = (_pcm_param_min_hi > 1) ? 1 : (_pcm_param_min_hi < 0) ? 0 : _pcm_param_min_hi;
        break;

        case PCM_DEBUG_PARAM_MIN_LO:
            _pcm_param_min_lo += amount;
            _pcm_param_min_lo = (_pcm_param_min_lo > 1) ? 1 : (_pcm_param_min_lo < 0) ? 0 : _pcm_param_min_lo;
        break;
    }

    pcm_init(-1);
}

uint16_t pcm_frequency_to_fixedpoint_increment(float frequency)
{
    // Convert frequency to fixed point increment
    float increment = (frequency * PCM_SINE_TABLE_SIZE) / (float) PCM_SAMPLE_RATE;
    return (uint16_t)(increment * PCM_FREQUENCY_SHIFT_FIXED + 0.5f);
}

uint16_t pcm_amplitude_to_fixedpoint(float input) {
    uint16_t tmp = (uint16_t)(input * PCM_AMPLITUDE_SHIFT_FIXED);
    if(input>0 && !tmp) tmp = 1;
    return tmp;
 }

// Initialize the sine table
void _generate_sine_table(float scaler)
{
    // Ensure scaler is within bounds
    scaler = (scaler > 1.0f) ? 1.0f : (scaler < 0.0f) ? 0.0f : scaler;

    float inc = TWO_PI / PCM_SINE_TABLE_SIZE;
    float fi = 0;

    // Generate 256 entries to cover a full sine wave cycle
    for (int i = 0; i < PCM_SINE_TABLE_SIZE; i++)
    {
        float sample = sinf(fi);

        // Convert to int16_t, rounding to nearest value
        _pcm_sine_table[i] = (int16_t)(sample * ((float) PCM_WRAP_HALF_VAL * scaler + 0.5f));

        fi += inc;
        fi = fmodf(fi, TWO_PI);  
    }
}

typedef struct 
{
    int16_t queue[PCM_RAW_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint16_t count;
} pcm_raw_queue_t;

volatile pcm_raw_queue_t _pcm_raw_queue = {0};

void pcm_raw_queue_init(pcm_raw_queue_t *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

int16_t pcm_raw_queue_count()
{
    return _pcm_raw_queue.count;
}

int16_t pcm_raw_queue_push(int16_t *data, uint16_t len)
{
    if (_pcm_raw_queue.count >= PCM_RAW_QUEUE_SIZE)
    {
        return -1; // Queue is full
    }

    for(uint16_t i = 0; i < len; i++)
    {
        if (_pcm_raw_queue.count >= PCM_RAW_QUEUE_SIZE)
        {
            return false; // Queue is full
        }
        _pcm_raw_queue.queue[_pcm_raw_queue.tail] = data[i];
        _pcm_raw_queue.tail = (_pcm_raw_queue.tail + 1) % PCM_RAW_QUEUE_SIZE;
        _pcm_raw_queue.count++;
    }

    return (int16_t) _pcm_raw_queue.count;
}

// Pop values from the queue
bool pcm_raw_queue_pop(int16_t *out)
{
    if (_pcm_raw_queue.count == 0)
    {
        return false; // Queue is empty
    }

    *out = _pcm_raw_queue.queue[_pcm_raw_queue.head];
    _pcm_raw_queue.head = (_pcm_raw_queue.head + 1) % PCM_RAW_QUEUE_SIZE;
    _pcm_raw_queue.count--;
    return true;
}

#define PCM_AMFM_QUEUE_SIZE 128 // Adjust size as needed


typedef struct
{
    haptic_processed_s buffer[PCM_AMFM_QUEUE_SIZE];
    uint8_t sample_count[PCM_AMFM_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} pcm_amfm_queue_t;

pcm_amfm_queue_t _pcm_amfm_queue = {0};

SNAPSHOT_TYPE(haptic_packet, haptic_packet_s);
snapshot_haptic_packet_t _haptic_snap;

// Initialize the queue
void pcm_amfm_queue_init()
{
    _pcm_amfm_queue.head = 0;
    _pcm_amfm_queue.tail = 0;
    _pcm_amfm_queue.count = 0;
}

#define DEFAULT_HI (uint16_t)(((75.0f * PCM_SINE_TABLE_SIZE) / PCM_SAMPLE_RATE) * PCM_FREQUENCY_SHIFT_FIXED + 0.5)
#define DEFAULT_LO (uint16_t)(((35.0f * PCM_SINE_TABLE_SIZE) / PCM_SAMPLE_RATE) * PCM_FREQUENCY_SHIFT_FIXED + 0.5)

// Push new packet with all pairs to snapshot
// Returns true if successful
bool pcm_amfm_push(haptic_packet_s *packet)
{
    const uint16_t default_hi = DEFAULT_HI; // 75Hz
    const uint16_t default_lo = DEFAULT_LO; // 35Hz

    // Set default frequency increment values for all pairs
    for(int i = 0; i < packet->count && i < 3; i++)
    {
        if(packet->pairs[i].hi_frequency_increment < default_hi)
            packet->pairs[i].hi_frequency_increment = default_hi;
        if(packet->pairs[i].lo_frequency_increment < default_lo)
            packet->pairs[i].lo_frequency_increment = default_lo;
    }

    static uint64_t counter = 0;
    counter++;

    packet->counter = counter;

    snapshot_haptic_packet_write(&_haptic_snap, packet);
    return true;
}

// Pop packet from snapshot
bool pcm_amfm_pop(haptic_packet_s *out)
{
    snapshot_haptic_packet_read(&_haptic_snap, out);
    return true;
}

// Helper function to check if queue is empty
bool pcm_amfm_is_empty()
{
    return _pcm_amfm_queue.count == 0;
}

// Helper function to check if queue is full
bool pcm_amfm_is_full()
{
    return _pcm_amfm_queue.count >= PCM_AMFM_QUEUE_SIZE;
}

typedef struct 
{
    float target_percent;
    float current_percent;
    bool  apply_brake;
    bool  erm_active;
} pcm_erm_state_s;

// Simulation handler where the PCM samples 
// represent ERM vibrations
// Simulation handler where the PCM samples 
// represent ERM vibrations with natural motor characteristics
// Uses unified percent control for realistic frequency/amplitude coupling
void _pcm_erm_handler(pcm_erm_state_s *state)
{
    // Time constants for exponential curves (smaller = faster response)
    const float time_constant_up   = 0.125f;  // Spin-up rate
    const float time_constant_down = 0.125f;  // Spin-down rate (faster due to friction)
    
    // Brake multiplier for faster deceleration
    const float brake_multiplier = 2.5f;

    // Frequency range mapping
    const float min_freq_lo = 40.0f;   // Minimum frequency when motor starts
    const float min_freq_hi = 40.0f;  // Minimum frequency when motor starts
    const float target_freq_lo = 85.0f;
    const float target_freq_hi = 170.0f;
    
    // Amplitude range mapping
    const float min_amp_lo = 0.0f;     // Zero amplitude when motor stops
    const float min_amp_hi = 0.0f;     // Zero amplitude when motor stops  
    const float target_amp_lo = 0.1f;
    const float target_amp_hi = 0.1f;

    // Unified motor control with exponential curves
    float percent_diff = state->target_percent - state->current_percent;
    if (fabs(percent_diff) > 0.001f) // Avoid unnecessary computation for tiny differences
    {
        float time_constant;
        if (percent_diff > 0) {
            // Motor spinning up - overcomes inertia
            time_constant = time_constant_up;
        } else {
            // Motor spinning down - friction helps, brake can help more
            time_constant = time_constant_down;
            if (state->apply_brake) {
                time_constant *= brake_multiplier;
            }
        }
        
        // Exponential approach: current += (target - current) * time_constant
        state->current_percent += percent_diff * time_constant;
        
        // Clamp to target when very close (prevents oscillation)
        if (fabs(state->target_percent - state->current_percent) < 0.005f) {
            state->current_percent = state->target_percent;
        }
    }

    // Ensure we don't go below zero or above 1.0
    state->current_percent = fmaxf(0.0f, fminf(1.0f, state->current_percent));

    // Calculate actual frequencies and amplitudes based on unified percent
    // Both frequency and amplitude are naturally coupled to motor speed
    float actual_freq_lo = min_freq_lo + (target_freq_lo - min_freq_lo) * state->current_percent;
    float actual_freq_hi = min_freq_hi + (target_freq_hi - min_freq_hi) * state->current_percent;
    float actual_amp_lo = min_amp_lo + (target_amp_lo - min_amp_lo) * state->current_percent;
    float actual_amp_hi = min_amp_hi + (target_amp_hi - min_amp_hi) * state->current_percent;

    haptic_packet_s packet = {0};
    packet.count = 1;
    packet.pairs[0].hi_frequency_increment = pcm_frequency_to_fixedpoint_increment(actual_freq_hi);
    packet.pairs[0].lo_frequency_increment = pcm_frequency_to_fixedpoint_increment(actual_freq_lo);
    packet.pairs[0].hi_amplitude_fixed = pcm_amplitude_to_fixedpoint(actual_amp_hi);
    packet.pairs[0].lo_amplitude_fixed = pcm_amplitude_to_fixedpoint(actual_amp_lo);

    if(state->erm_active)
        pcm_amfm_push(&packet);

    if(state->current_percent <= 0.001f) // Motor stops when percent reaches near zero
    {
        state->erm_active = false;
    }
}
volatile bool _pcm_init_done = false;
void pcm_init(int intensity) 
{
    intensity = (intensity > 255) ? 255 : intensity;
    static uint8_t this_intensity = 0;

    if(!intensity)
    {
        _lo_amp_scaler_fixed = 0;
        _lo_amp_scaler_fixed_min = 0;
        _hi_amp_scaler_fixed = 0;
        _hi_amp_scaler_fixed_min = 0;
        this_intensity = intensity;
        return;
    }
    else if (intensity < 0)
    {
        // Use previous intensity
    }
    else 
    {
        this_intensity = intensity;
    }

    // The scaler based on our user config setting with logarithmic scaling
    // Maps 0-255 input to 0.0-1.0 output on a logarithmic curve
    float input_normalized = (float) this_intensity / 255.0f;
    float scaler;

    // Our actual target range is 165 units (starting at 90)
    if(this_intensity > 0)
    {
        input_normalized = (input_normalized * 0.65f) + 0.35f; // Scale to 0.35 - 1.0
        scaler = (input_normalized == 0.0f) ? 0.0f : powf(input_normalized, 2.0f);
    }
    else 
    {
        scaler = 0.0f; // If intensity is 0, set scaler to 0
    }

    // Calculate the scaled maximum wrap value. We also factor in the user config scaler and board config
    float max_scaled = ((float) PCM_WRAP_HALF_VAL * scaler * PCM_MAX_SAFE_RATIO);

    // Calculate the value our minimums will be
    // We only use half of the wrap value because the low/high amplitudes
    // are added together, and the maximum cannot exceed the half wrap value
    // This allows us to cleanly add the low and high amplitudes
    float pcm_wrap_minimum_lo = (float) (PCM_WRAP_HALF_VAL>>1) * _pcm_param_min_lo;
    float pcm_wrap_minimum_hi = (float) (PCM_WRAP_HALF_VAL>>1) * _pcm_param_min_hi;

    // Calculate remainder of range 
    float remaininghi = max_scaled - pcm_wrap_minimum_hi;
    float remaininglo = max_scaled - pcm_wrap_minimum_lo;

    if(remaininghi < pcm_wrap_minimum_hi)
    {
        // If the remaining high range is less than the minimum, set it to the minimum
        remaininghi = pcm_wrap_minimum_hi;
    }

    if(remaininglo < pcm_wrap_minimum_lo)
    {
        // If the remaining low range is less than the minimum, set it to the minimum
        remaininglo = pcm_wrap_minimum_lo;
    }

    // Calculate scalers for the remaining ranges for our sine table
    float loscale = (remaininglo / (float) PCM_WRAP_HALF_VAL);
    float hiscale = (remaininghi / (float) PCM_WRAP_HALF_VAL);

    // Calculate the max scaler for external samples (trigger feedback)
    _external_sample_scaler = (uint32_t) max_scaled / 255;

    // Calculate the fixed point scalers for our lo and hi amplitudes
    _lo_amp_scaler_fixed = (uint32_t) (loscale * PCM_AMPLITUDE_SHIFT_FIXED);
    _hi_amp_scaler_fixed = (uint32_t) (hiscale * PCM_AMPLITUDE_SHIFT_FIXED);

    float tmpminloscaler = pcm_wrap_minimum_lo / (float) PCM_WRAP_HALF_VAL;
    float tmpminhiscaler = pcm_wrap_minimum_hi / (float) PCM_WRAP_HALF_VAL;

    // Calculate the fixed point minimums for our lo and hi amplitudes
    _lo_amp_scaler_fixed_min  = (uint32_t) ((float) tmpminloscaler * (float) PCM_AMPLITUDE_SHIFT_FIXED);
    _hi_amp_scaler_fixed_min  = (uint32_t) ((float) tmpminhiscaler * (float) PCM_AMPLITUDE_SHIFT_FIXED);

    _generate_sine_table(scaler);

    if(_pcm_init_done) return;
    _pcm_init_done = true;
    pcm_amfm_queue_init();
}

static inline int16_t lerp_fixed_signed(int16_t start, int16_t end, uint8_t t)
{
    int32_t diff = (int32_t)end - (int32_t)start;  // Promote to int32_t to handle full range
    return (int16_t)(start + ((diff * t) >> 8));
}

static inline uint16_t lerp_fixed_unsigned(uint16_t start, uint16_t end, uint8_t t)
{
    int32_t diff = (int32_t)end - (int32_t)start;  // Use int32_t for diff to handle underflow
    return (uint16_t)(start + ((diff * t) >> 8));
}

uint8_t *_external_sample_l;
uint8_t *_external_sample_r;
uint32_t _external_sample_remaining_l = 0;
uint32_t _external_sample_remaining_r = 0;
uint32_t _external_sample_size_l = 0;
uint32_t _external_sample_size_r = 0;

#if defined(HOJA_HAPTICS_CHAN_SWAP) && (HOJA_HAPTICS_CHAN_SWAP==1)
void pcm_play_bump(bool right, bool left)
#else
void pcm_play_bump(bool left, bool right)
#endif
{
    if(!haptic_config->haptic_triggers) return;

    static bool left_on = false;
    static bool right_on = false;

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
    if(left && !left_on)
    {
        _external_sample_l = hapticPattern;
        _external_sample_remaining_l = sizeof(hapticPattern);
        _external_sample_size_l = sizeof(hapticPattern);
        left_on = true;
    }
    else if (left_on && !left)
    {
        _external_sample_l = offPattern;
        _external_sample_remaining_l = sizeof(offPattern);
        _external_sample_size_l = sizeof(offPattern);
        left_on = false;
    }

    if(right && !right_on)
    {
        _external_sample_r = hapticPattern;
        _external_sample_remaining_r = sizeof(hapticPattern);
        _external_sample_size_r = sizeof(hapticPattern);
        right_on = true;
    }
    else if (right_on && !right)
    {
        _external_sample_r = offPattern;
        _external_sample_remaining_r = sizeof(offPattern);
        _external_sample_size_r = sizeof(offPattern);
        right_on = false;
    }
#endif
}

pcm_erm_state_s _pcm_erm_state = {
    .target_percent = 0.0f,
    .current_percent = 0.0f,
    .apply_brake = false,
    .erm_active = false,
};

void pcm_erm_set(uint8_t intensity, bool brake)
{
    if(!intensity)
    {
        // If intensity is 0, reset the state
        _pcm_erm_state.target_percent = 0.0f;
    }
    else 
    {
        // Set the target frequency and amplitude based on intensity
        _pcm_erm_state.target_percent = (float)intensity / 255.0f;
        _pcm_erm_state.erm_active = true;
    }
    
    // Set the brake state
    _pcm_erm_state.apply_brake = brake;
}

static inline bool _is_haptic_packet_different(haptic_packet_s *a, haptic_packet_s *b)
{
    if(a->counter != b->counter) return true;
    return false;
}

// Generate PCM_BUFFER_SIZE samples of PCM data
void pcm_generate_buffer(uint32_t *buffer)
{
    // --- Persistent State ---
    static uint32_t phase_hi = 0;
    static uint32_t phase_lo = 0;
    
    static haptic_packet_s current_packet     = {0};
    static uint8_t         current_pair_idx   = 0;
    static uint16_t        samples_remaining  = 0; // Tracks duration of current pair
    static bool            is_active          = false;

    // Synthesis Parameters (Latch these to prevent mid-sample clicking)
    static uint32_t hi_freq_inc = 200;
    static uint32_t lo_freq_inc = 200;
    static uint32_t hi_amp_scaler = 0;
    static uint32_t lo_amp_scaler = 0;
    static uint32_t target_dc_offset = 0;
    static uint32_t dc_offset = 0;

    // 1. ERM Processing (optional)
    if(_pcm_erm_state.erm_active)
    {
        _pcm_erm_handler(&_pcm_erm_state);
    }

    // 2. Synthesis Loop (64 Samples)
    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
        // Check if we need to load a new pair or a new packet
        // We do this inside the loop to ensure sample-accurate timing 
        // even if a pair duration doesn't align perfectly with the 64-sample buffer boundary
        if (samples_remaining == 0)
        {
            haptic_packet_s inbound_packet = {0};
            pcm_amfm_pop(&inbound_packet);

            // CASE A: New data has arrived (Counter mismatch)
            if (_is_haptic_packet_different(&inbound_packet, &current_packet))
            {
                current_packet     = inbound_packet;
                current_pair_idx   = 0;
                is_active          = (current_packet.count > 0);
            }
            // CASE B: Current packet sequence still has pairs left
            else if (is_active && (current_pair_idx < (current_packet.count - 1)))
            {
                current_pair_idx++;
            }
            // CASE C: We are at the end of a sequence (Hold/Idle)
            else 
            {
                // If the last pair was 0 amplitude, we are truly inactive
                if (current_packet.pairs[current_pair_idx].hi_amplitude_fixed == 0 && 
                    current_packet.pairs[current_pair_idx].lo_amplitude_fixed == 0)
                {
                    is_active = false;
                }
                // We keep samples_remaining at 0 here so we check for new packets 
                // every sample until one arrives.
            }

            // Update local synthesis parameters from the current pair
            if (is_active)
            {
                haptic_processed_s *v = &current_packet.pairs[current_pair_idx];
                
                hi_freq_inc = v->hi_frequency_increment;
                lo_freq_inc = v->lo_frequency_increment;

                // Scaling logic from oldpcm
                hi_amp_scaler = (v->hi_amplitude_fixed) ? 
                    ((v->hi_amplitude_fixed * _hi_amp_scaler_fixed) >> PCM_AMPLITUDE_BIT_SCALE) + _hi_amp_scaler_fixed_min : 0;
                
                lo_amp_scaler = (v->lo_amplitude_fixed) ? 
                    ((v->lo_amplitude_fixed * _lo_amp_scaler_fixed) >> PCM_AMPLITUDE_BIT_SCALE) + _lo_amp_scaler_fixed_min : 0;

                uint32_t hi_peak = (hi_amp_scaler * PCM_WRAP_HALF_VAL) >> PCM_AMPLITUDE_BIT_SCALE;
                uint32_t lo_peak = (lo_amp_scaler * PCM_WRAP_HALF_VAL) >> PCM_AMPLITUDE_BIT_SCALE;
                target_dc_offset = (hi_peak + lo_peak);

                // Determine how many samples this specific pair should last
                // If count is 3, we use the smaller chunk sizes to fit the 8ms window
                if (current_packet.count == 3) samples_remaining = 63/3;
                else if (current_packet.count == 2) samples_remaining = 63/2;
                else samples_remaining = 63;
            }
            else 
            {
                hi_amp_scaler = 0;
                lo_amp_scaler = 0;
                target_dc_offset = 0;
                // Avoid infinite loop if inactive
                samples_remaining = PCM_BUFFER_SIZE; 
            }
        }

        // 3. Waveform Synthesis
        uint16_t idx_hi = (phase_hi >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;
        uint16_t idx_lo = (phase_lo >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;

        int16_t sine_hi = _pcm_sine_table[idx_hi];
        int16_t sine_lo = _pcm_sine_table[idx_lo];

        // Apply signs and scaling
        int32_t scaled_hi = ((int32_t)((sine_hi >= 0 ? sine_hi : -sine_hi) * hi_amp_scaler) >> PCM_AMPLITUDE_BIT_SCALE);
        int32_t scaled_lo = ((int32_t)((sine_lo >= 0 ? sine_lo : -sine_lo) * lo_amp_scaler) >> PCM_AMPLITUDE_BIT_SCALE);
        
        if (sine_hi < 0) scaled_hi = -scaled_hi;
        if (sine_lo < 0) scaled_lo = -scaled_lo;

        int32_t mixed = (scaled_hi + scaled_lo);

        // Ramp DC offset (Attack/Decay)
        if (dc_offset < target_dc_offset) dc_offset+=2;
        else if (dc_offset > target_dc_offset) dc_offset--;

        mixed += dc_offset;
        if (mixed < 0) mixed = 0;

        // External/Trigger Sample Mixing
        uint32_t ext_l = 0, ext_r = 0;
        if (_external_sample_remaining_l) {
            ext_l = (uint32_t)_external_sample_l[_external_sample_size_l - _external_sample_remaining_l] * _external_sample_scaler;
            _external_sample_remaining_l--;
        }
        if (_external_sample_remaining_r) {
            ext_r = (uint32_t)_external_sample_r[_external_sample_size_r - _external_sample_remaining_r] * _external_sample_scaler;
            _external_sample_remaining_r--;
        }

        // Raw PCM Override Logic
        static bool load_new_raw = false;
        int16_t pcm_raw_val = 0;
        if (load_new_raw && pcm_raw_queue_pop(&pcm_raw_val)) {
            if (pcm_raw_val >= 0) mixed = (pcm_raw_val >> 3);
        }
        load_new_raw = !load_new_raw;

        // Final Output
        buffer[i] = (((uint32_t)(mixed + ext_l)) << 16) | ((uint32_t)(mixed + ext_r));

        // Advance phases and decrement timers
        phase_hi = (phase_hi + hi_freq_inc) % PCM_SINE_WRAPAROUND;
        phase_lo = (phase_lo + lo_freq_inc) % PCM_SINE_WRAPAROUND;
        
        if (samples_remaining > 0) samples_remaining--;
    }
}
