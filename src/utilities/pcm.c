#include "utilities/pcm.h"
#include "hal/mutex_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "switch/switch_haptics.h"

#include "hoja_bsp.h"
#if HOJA_BSP_CHIPSET == CHIPSET_RP2040
// Special float functions for RP2040
#include "pico/float.h"
#endif

int16_t _pcm_sine_table[PCM_SINE_TABLE_SIZE];
volatile uint32_t _lo_amp_minimum = 0;
volatile uint32_t _hi_amp_minimum  = 0;

float _global_fixed_scaler = 1.0f;

uint32_t _pcm_max_safe_value    = 0; 
uint32_t _pcm_max_safe_scaler   = 0; 
uint32_t _pcm_sample_scaler = 0;

float _pcm_max_safe_hi = 0; 
float _pcm_max_safe_lo = 0; 

float _pcm_param_min_hi = PCM_LO_FREQUENCY_MIN;
float _pcm_param_min_lo = PCM_LO_FREQUENCY_MIN;

float _pcm_param_max = PCM_MAX_SAFE_RATIO;

#define TWO_PI 2.0f * M_PI

void pcm_debug_adjust_param(uint8_t param_type, float amount)
{
    switch(param_type)
    {
        case PCM_DEBUG_PARAM_MIN_HI:
            _pcm_param_min_hi += amount;
            _pcm_param_min_hi = (_pcm_param_min_hi > 1) ? 1 : (_pcm_param_min_hi < 0) ? 0 : _pcm_param_min_hi;
        break;

        case PCM_DEBUG_PARAM_MAX:
            _pcm_param_max += amount;
            _pcm_param_max = (_pcm_param_max > 1) ? 1 : (_pcm_param_max < 0) ? 0 : _pcm_param_max;
        break;

        case PCM_DEBUG_PARAM_MIN_LO:
            _pcm_param_min_lo += amount;
            _pcm_param_min_lo = (_pcm_param_min_lo > 1) ? 1 : (_pcm_param_min_lo < 0) ? 0 : _pcm_param_min_lo;
        break;
    }

    pcm_init(-1);
}

// Initialize the sine table
void _generate_sine_table()
{
    float inc = TWO_PI / PCM_SINE_TABLE_SIZE;
    float fi = 0;

    // Generate 256 entries to cover a full sine wave cycle
    for (int i = 0; i < PCM_SINE_TABLE_SIZE; i++)
    {
        float sample = sinf(fi);

        // Convert to int16_t, rounding to nearest value
        _pcm_sine_table[i] = (int16_t)(sample * (float) PCM_WRAP_VAL);

        fi += inc;
        fi = fmodf(fi, TWO_PI);  
    }
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

void pcm_set_frequency_amplitude(uint16_t frequency_step_value, int16_t amplitude_fixed) 
{
    haptic_processed_s value = {
        .lo_amplitude_fixed = amplitude_fixed, 
        .hi_amplitude_fixed = 0, 
        .lo_frequency_increment = frequency_step_value, 
        .hi_frequency_increment = 0
        };

    pcm_amfm_push(&value);
}

// Initialize the queue
void pcm_amfm_queue_init()
{
    _pcm_amfm_queue.head = 0;
    _pcm_amfm_queue.tail = 0;
    _pcm_amfm_queue.count = 0;
}

#define DEFAULT_HI (uint16_t)(((75.0f * PCM_SINE_TABLE_SIZE) / PCM_SAMPLE_RATE) * PCM_FREQUENCY_SHIFT_FIXED + 0.5)
#define DEFAULT_LO (uint16_t)(((35.0f * PCM_SINE_TABLE_SIZE) / PCM_SAMPLE_RATE) * PCM_FREQUENCY_SHIFT_FIXED + 0.5)

// Push new values to queue
// Returns true if successful, false if queue is full
bool pcm_amfm_push(haptic_processed_s *value)
{
    if (_pcm_amfm_queue.count >= PCM_AMFM_QUEUE_SIZE)
    {
        return false; // Queue is full
    }

    const uint16_t default_hi = DEFAULT_HI; // 75Hz
    const uint16_t default_lo = DEFAULT_LO; // 35Hz 

    // Set default frequency increment values so we're always incrementing
    if(value->hi_frequency_increment < default_hi) value->hi_frequency_increment = default_hi; 
    if(value->lo_frequency_increment < default_lo) value->lo_frequency_increment = default_lo; 

    // Always 3 samples
    memcpy(&_pcm_amfm_queue.buffer[_pcm_amfm_queue.tail], value, sizeof(haptic_processed_s));
    _pcm_amfm_queue.tail = (_pcm_amfm_queue.tail + 1) % PCM_AMFM_QUEUE_SIZE;
    _pcm_amfm_queue.count++;
    return true;
}

// Pop values from queue
// Returns sample count if successful, false if queue is empty
bool pcm_amfm_pop(haptic_processed_s *out)
{
    if (_pcm_amfm_queue.count == 0)
    {
        return false; // Queue is empty
    }

    memcpy(out, &_pcm_amfm_queue.buffer[_pcm_amfm_queue.head], sizeof(haptic_processed_s));
    _pcm_amfm_queue.buffer[_pcm_amfm_queue.head].hi_amplitude_fixed = 0;
    _pcm_amfm_queue.buffer[_pcm_amfm_queue.head].lo_amplitude_fixed = 0;

    _pcm_amfm_queue.head = (_pcm_amfm_queue.head + 1) % PCM_AMFM_QUEUE_SIZE;
    _pcm_amfm_queue.count--;
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

volatile bool _pcm_init_done = false;
void pcm_init(int intensity) 
{
    intensity = (intensity > 255) ? 255 : intensity;
    static uint8_t this_intensity = 0;

    if(!intensity)
    {
        _lo_amp_minimum = 0;
        _hi_amp_minimum = 0;
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

    float scaler = (float) this_intensity / 255.0f;
    _global_fixed_scaler = scaler;

    // Calculate pcm clamp value (max allowed value)
    _pcm_max_safe_value     = (uint32_t) ((float) PCM_WRAP_VAL * _pcm_param_max);
    _pcm_max_safe_scaler    = (uint32_t) (_pcm_param_max * PCM_AMPLITUDE_SHIFT_FIXED);

    _pcm_sample_scaler = (uint32_t) _pcm_max_safe_value / 255;

    // Minimum amplitudes are based on our WRAP value

    // Calculate the exact wrap value that our min amplitudes
    // rest at 
    float lomin_pcm = (float) PCM_WRAP_VAL * _pcm_param_min_lo;
    float himin_pcm = (float) PCM_WRAP_VAL * _pcm_param_min_hi;

    // Calculate remainder of range 
    float remaininghi = (float) _pcm_max_safe_value - himin_pcm;
    float remaininglo = (float) _pcm_max_safe_value - lomin_pcm;

    _lo_amp_minimum  = (uint32_t) (_pcm_param_min_lo * PCM_AMPLITUDE_SHIFT_FIXED);
    _hi_amp_minimum  = (uint32_t) (_pcm_param_min_hi * PCM_AMPLITUDE_SHIFT_FIXED);

    if(_pcm_init_done) return;
    _pcm_init_done = true;
    _generate_sine_table();
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

uint8_t *_external_sample;
uint32_t _external_sample_remaining = 0;
uint32_t _external_sample_size = 0;

void pcm_play_sample(uint8_t *sample, uint32_t size) 
{
    _external_sample = sample;
    _external_sample_remaining = size;
    _external_sample_size = size;
}

// Generate PCM_BUFFER_SIZE samples of PCM data
void pcm_generate_buffer(
    uint32_t *buffer // Output buffer
)
{
    // Local variables for phase accumulators
    static uint32_t phase_hi = 0;
    static uint32_t phase_lo = 0;
    static uint16_t current_sample_idx = 0;
    static bool     processing_sample = false;

    static uint32_t last_hi_amp = 0;
    static uint32_t last_lo_amp = 0;

    static haptic_processed_s   current_values  = {0};

    static int load_sample = -1;

    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
        uint16_t idx_hi = (phase_hi >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;
        uint16_t idx_lo = (phase_lo >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;

        static uint32_t hi_frequency_increment = 200;
        static uint32_t lo_frequency_increment = 200;
        static uint32_t hi_amp_scaler = 0;
        static uint32_t lo_amp_scaler = 0;
        static uint32_t hi_amp_peak = 0;
        static uint32_t lo_amp_peak = 0;

        if(!processing_sample)
        {
            // Get new values from queue
            if (!pcm_amfm_is_empty())
            {
                pcm_amfm_pop(&current_values);
                processing_sample = true;
                current_sample_idx = 0;

                /*
                    High frequency is used for Special hit effects, tilt attack hit, tilt whiff,
                    Smash attack chargeup, enemy death confirmation, Shield, Landing
                */
                hi_frequency_increment = current_values.hi_frequency_increment;

                if(current_values.hi_amplitude_fixed)
                {
                    float tmphia = _global_fixed_scaler * (float) current_values.hi_amplitude_fixed;
                    current_values.hi_amplitude_fixed = (uint32_t) tmphia;

                    hi_amp_scaler = current_values.hi_amplitude_fixed + _hi_amp_minimum;
                    hi_amp_peak = (current_values.hi_amplitude_fixed * PCM_WRAP_VAL) >> PCM_AMPLITUDE_BIT_SCALE;
                }
                else 
                {
                    hi_amp_scaler = 0;
                    hi_amp_peak = 0;
                }

                // If we have a low amplitude, we need to set the scaler
                // to the minimum value
                if(current_values.lo_amplitude_fixed)
                {
                    float tmpla = _global_fixed_scaler * (float) current_values.lo_amplitude_fixed;
                    current_values.lo_amplitude_fixed = (uint32_t) tmpla;

                    lo_amp_scaler = current_values.lo_amplitude_fixed + _lo_amp_minimum;
                    lo_amp_peak = (current_values.lo_amplitude_fixed * PCM_WRAP_VAL) >> PCM_AMPLITUDE_BIT_SCALE;
                }
                else 
                {
                    lo_amp_scaler = 0;
                    lo_amp_peak = 0;
                }

                /*
                    Low frequency is used for Jump, Smash attack, 
                    Attack hit, Tilt IMPACT, Dash, Walk, Special, Shield, Air dodge
                */
                lo_frequency_increment = current_values.lo_frequency_increment;

                lo_amp_scaler = current_values.lo_amplitude_fixed;
            }
        }

        uint32_t this_hi_scaler = hi_amp_scaler;
        uint32_t this_lo_scaler = lo_amp_scaler;

        int16_t sine_hi = _pcm_sine_table[idx_hi];
        int16_t sine_lo = _pcm_sine_table[idx_lo];

        bool hi_sign = (sine_hi >= 0) ? true : false;
        bool lo_sign = (sine_lo >= 0) ? true : false;

        if(!hi_sign) sine_hi = -sine_hi;
        if(!lo_sign) sine_lo = -sine_lo;

        int32_t scaled_hi = 0;
        int32_t scaled_lo = 0;
        
        if(hi_sign && lo_sign)
        {
            // Both signs are the same, we can check 
            // if we need to perform scaling to prevent clipping
            uint32_t combined_amp = this_hi_scaler + this_lo_scaler;
            if(combined_amp > _pcm_max_safe_scaler)
            {
                // Compute the scaling factor as a fixed-point ratio
                uint32_t new_ratio = _pcm_max_safe_scaler / combined_amp;
                // Scale the components proportionally
                this_hi_scaler = (this_hi_scaler * new_ratio) >> PCM_AMPLITUDE_BIT_SCALE;
                this_lo_scaler = (this_lo_scaler * new_ratio) >> PCM_AMPLITUDE_BIT_SCALE;
            }
        }
        else if(hi_sign != lo_sign)
        {

            // Here we should predict if the sine wave will be destroyed
            if(hi_sign)
            {
                int32_t dif = hi_amp_peak - lo_amp_peak;
                if(dif <= 0)
                {
                    // Negate the low amplitude
                    this_lo_scaler = 0;
                }
            }
            else 
            {
                int32_t dif = lo_amp_peak - hi_amp_peak;
                if(dif <= 0)
                {
                    // Negate the high amplitude
                    this_hi_scaler = 0;
                }
            }
        }
        
        scaled_hi = ((uint32_t) sine_hi * this_hi_scaler) >> PCM_AMPLITUDE_BIT_SCALE; 
        scaled_lo = ((uint32_t) sine_lo * this_lo_scaler) >> PCM_AMPLITUDE_BIT_SCALE; 

        uint32_t external_sample = 0;

        // Mix in external sample if we have any remaining
        if(_external_sample_remaining)
        {
            external_sample = (uint32_t) _external_sample[_external_sample_size - _external_sample_remaining] * _pcm_sample_scaler;
            _external_sample_remaining--;
        }

        // Debug disable lo 
        //scaled_lo = 0;

        // Debug disable hi
        //scaled_hi = 0;

        // Reappy signs
        if(!hi_sign) scaled_hi = -scaled_hi;
        if(!lo_sign) scaled_lo = -scaled_lo;

        // Mix the two channels
        int32_t    mixed = (scaled_hi + scaled_lo);

        if(external_sample)
        {
            mixed += external_sample;
        }

        if(mixed < 0) mixed = 0;

        uint32_t outsample = ((uint32_t) mixed << 16) | (uint32_t) mixed;
        buffer[i] = outsample;

        phase_hi = (phase_hi + hi_frequency_increment) % PCM_SINE_WRAPAROUND;
        phase_lo = (phase_lo + lo_frequency_increment) % PCM_SINE_WRAPAROUND;

        if(processing_sample)
        {
            current_sample_idx++;
            if(current_sample_idx >= PCM_SAMPLES_PER_PAIR)
            {
                processing_sample = false;
            }
        }
    }
}