#include "utilities/pcm.h"
#include "hal/mutex_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "utilities/pcm_samples.h"
#include "utilities/settings.h"

#include "hoja.h"

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

// Amplitude ceiling (0..1). Defaults to the library value; overridden at the
// first pcm_init() from hoja_config_s.haptics.intensity_max when non-zero.
volatile float _pcm_max_safe_ratio = PCM_MAX_SAFE_RATIO;

#define PCM_LO_FREQUENCY_LPF_HZ 400
#define PCM_HI_FREQUENCY_LPF_HZ 1000

#define TWO_PI 2.0f * M_PI

uint16_t _pcm_fp_amplitude_multiplier_table[256] = {0};
uint16_t _pcm_fp_hi_frequency_table[128] = {0};
uint16_t _pcm_fp_lo_frequency_table[128] = {0};

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
void _pcm_generate_sine_table(float scaler)
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

void _pcm_generate_all_tables(void)
{
    ns_api_generate_fp_amplitude_multiplier_table(PCM_AMPLITUDE_SHIFT_FIXED, _pcm_fp_amplitude_multiplier_table);
    ns_api_generate_fp_haptic_frequency_tables(PCM_FREQUENCY_SHIFT_FIXED, PCM_SINE_TABLE_SIZE, PCM_SAMPLE_RATE, 
        _pcm_fp_hi_frequency_table, _pcm_fp_lo_frequency_table);
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

// Official software PCM ring is 160 samples, 20 per pair (8 grains).
// vibrationProcess refuses a pair when GetFreePcmSampleCount() <= 19.
#define PCM_AMFM_QUEUE_SIZE 8
#define PCM_OFFICIAL_SAMPLES_PER_GRAIN 20
#define PCM_OFFICIAL_RING_SAMPLES 160
#define PCM_OFFICIAL_HOLD_GRAINS 201

static uint8_t _pcm_vibrator_strobe = 0;
static volatile uint8_t _pcm_vibrator_nibble = 0;
// Live unpack zeros the GetNextVibration grain counter (cuts sustain).
static volatile bool _pcm_cut_hold = false;

typedef struct
{
    haptic_processed_s pair;
    bool note_on;
} pcm_amfm_item_t;

typedef struct
{
    pcm_amfm_item_t buffer[PCM_AMFM_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} pcm_amfm_queue_t;

static pcm_amfm_queue_t _pcm_amfm_queue = {0};
MUTEX_HAL_INIT(_pcm_amfm_mtx);

static void pcm_vibrator_refresh_nibble_locked(void)
{
    uint16_t used = (uint16_t)_pcm_amfm_queue.count * PCM_OFFICIAL_SAMPLES_PER_GRAIN;
    uint8_t fill;

    if ((used * 4u) < PCM_OFFICIAL_RING_SAMPLES)
    {
        fill = (uint8_t)((used * 4u) / PCM_OFFICIAL_SAMPLES_PER_GRAIN);
    }
    else
    {
        fill = 7;
    }
    if (fill > 7)
    {
        fill = 7;
    }
    _pcm_vibrator_nibble = (uint8_t)(fill | ((_pcm_vibrator_strobe ? 1u : 0u) << 3));
}

static void pcm_amfm_flush_locked(void)
{
    _pcm_amfm_queue.head = 0;
    _pcm_amfm_queue.tail = 0;
    _pcm_amfm_queue.count = 0;
    _pcm_cut_hold = false;
    pcm_vibrator_refresh_nibble_locked();
}

static void pcm_amfm_queue_init(void)
{
    MUTEX_HAL_ENTER_BLOCKING(&_pcm_amfm_mtx);
    _pcm_amfm_queue.head = 0;
    _pcm_amfm_queue.tail = 0;
    _pcm_amfm_queue.count = 0;
    _pcm_vibrator_strobe = 0;
    _pcm_cut_hold = false;
    pcm_vibrator_refresh_nibble_locked();
    MUTEX_HAL_EXIT(&_pcm_amfm_mtx);
}

// Official vibrationProcess: if the ring cannot take this pair, drop it
// and the rest of that report.
static bool pcm_amfm_push_pair_locked(const haptic_processed_s *pair, bool note_on)
{
    if (_pcm_amfm_queue.count >= PCM_AMFM_QUEUE_SIZE)
    {
        return false;
    }
    _pcm_amfm_queue.buffer[_pcm_amfm_queue.tail].pair = *pair;
    _pcm_amfm_queue.buffer[_pcm_amfm_queue.tail].note_on = note_on;
    _pcm_amfm_queue.tail = (uint8_t)((_pcm_amfm_queue.tail + 1u) % PCM_AMFM_QUEUE_SIZE);
    _pcm_amfm_queue.count++;
    return true;
}

static uint32_t pcm_pair_energy(const haptic_processed_s *pair)
{
    return (uint32_t)pair->hi_amplitude_fixed + (uint32_t)pair->lo_amplitude_fixed;
}

static bool pcm_amfm_try_pop_pair(haptic_processed_s *out, bool *note_on)
{
    MUTEX_HAL_ENTER_BLOCKING(&_pcm_amfm_mtx);
    if (_pcm_amfm_queue.count == 0)
    {
        MUTEX_HAL_EXIT(&_pcm_amfm_mtx);
        return false;
    }
    *out = _pcm_amfm_queue.buffer[_pcm_amfm_queue.head].pair;
    *note_on = _pcm_amfm_queue.buffer[_pcm_amfm_queue.head].note_on;
    _pcm_amfm_queue.head = (uint8_t)((_pcm_amfm_queue.head + 1u) % PCM_AMFM_QUEUE_SIZE);
    _pcm_amfm_queue.count--;
    // GetNextVibration sets strobe when used < 5 (ring nearly empty).
    if (_pcm_amfm_queue.count == 0)
    {
        _pcm_vibrator_strobe = 1;
    }
    pcm_vibrator_refresh_nibble_locked();
    MUTEX_HAL_EXIT(&_pcm_amfm_mtx);
    return true;
}

uint8_t ns_api_hook_get_vibrator_nibble(void)
{
    return _pcm_vibrator_nibble;
}

typedef struct
{
    float target_percent;
    float current_percent;
    bool  apply_brake;
    bool  erm_active;
} pcm_erm_state_s;

// Standard rumble on LRA: same exponential envelope as before the HD FIFO
// work (0.125 of remaining error per 8 ms buffer, linear freq/amp). Writes a
// live pair for the mixer. Must not use pcm_amfm_push (peak snaps + 201-grain
// hold turn a motor ramp into clicks and a long sustain tail).
static haptic_processed_s _pcm_erm_pair = {0};
static pcm_erm_state_s _pcm_erm_state = {
    .target_percent = 0.0f,
    .current_percent = 0.0f,
    .apply_brake = false,
    .erm_active = false,
};

static bool pcm_erm_driving(void)
{
    return _pcm_erm_state.erm_active || (_pcm_erm_state.current_percent > 0.001f);
}

static void pcm_erm_cancel(void)
{
    _pcm_erm_state.erm_active = false;
    _pcm_erm_state.target_percent = 0.0f;
    _pcm_erm_state.current_percent = 0.0f;
    memset(&_pcm_erm_pair, 0, sizeof(_pcm_erm_pair));
}

static void pcm_erm_handler(pcm_erm_state_s *state)
{
    const float time_constant_up   = 0.125f;
    const float time_constant_down = 0.125f;
    const float brake_multiplier = 2.5f;

    const float min_freq_lo = 40.0f;
    const float min_freq_hi = 40.0f;
    const float target_freq_lo = 85.0f;
    const float target_freq_hi = 170.0f;

    const float min_amp_lo = 0.0f;
    const float min_amp_hi = 0.0f;
    const float target_amp_lo = 0.1f;
    const float target_amp_hi = 0.1f;

    float percent_diff = state->target_percent - state->current_percent;
    if (fabsf(percent_diff) > 0.001f)
    {
        float time_constant;
        if (percent_diff > 0.0f)
        {
            time_constant = time_constant_up;
        }
        else
        {
            time_constant = time_constant_down;
            if (state->apply_brake)
            {
                time_constant *= brake_multiplier;
            }
        }

        state->current_percent += percent_diff * time_constant;

        if (fabsf(state->target_percent - state->current_percent) < 0.005f)
        {
            state->current_percent = state->target_percent;
        }
    }

    state->current_percent = fmaxf(0.0f, fminf(1.0f, state->current_percent));

    float actual_freq_lo = min_freq_lo + (target_freq_lo - min_freq_lo) * state->current_percent;
    float actual_freq_hi = min_freq_hi + (target_freq_hi - min_freq_hi) * state->current_percent;
    float actual_amp_lo = min_amp_lo + (target_amp_lo - min_amp_lo) * state->current_percent;
    float actual_amp_hi = min_amp_hi + (target_amp_hi - min_amp_hi) * state->current_percent;

    _pcm_erm_pair.hi_frequency_increment = pcm_frequency_to_fixedpoint_increment(actual_freq_hi);
    _pcm_erm_pair.lo_frequency_increment = pcm_frequency_to_fixedpoint_increment(actual_freq_lo);
    _pcm_erm_pair.hi_amplitude_fixed = pcm_amplitude_to_fixedpoint(actual_amp_hi);
    _pcm_erm_pair.lo_amplitude_fixed = pcm_amplitude_to_fixedpoint(actual_amp_lo);

    if (state->current_percent <= 0.001f)
    {
        state->erm_active = false;
        state->current_percent = 0.0f;
        memset(&_pcm_erm_pair, 0, sizeof(_pcm_erm_pair));
    }
}

void pcm_ns_to_fp(ns_haptics_packet_raw_s *in, haptic_packet_s *out)
{
    for(int i = 0; i < in->sample_count; i++)
    {
        out->pairs[i].hi_amplitude_fixed = _pcm_fp_amplitude_multiplier_table[in->samples[i].hi_amplitude_idx];
        out->pairs[i].lo_amplitude_fixed = _pcm_fp_amplitude_multiplier_table[in->samples[i].lo_amplitude_idx];

        out->pairs[i].hi_frequency_increment = _pcm_fp_hi_frequency_table[in->samples[i].hi_frequency_idx];
        out->pairs[i].lo_frequency_increment = _pcm_fp_lo_frequency_table[in->samples[i].lo_frequency_idx];
    }

    out->count = in->sample_count;
}

bool pcm_amfm_push(haptic_packet_s *packet)
{
    if (!packet)
    {
        return false;
    }

    // HD rumble owns the LRA. Drop any in-flight ERM sim so leftover
    // motor output cannot keep synthesizing after Switch HD starts.
    pcm_erm_cancel();

    MUTEX_HAL_ENTER_BLOCKING(&_pcm_amfm_mtx);
    // UnpackAmFmCodes clears strobe on every ingested rumble word, including
    // a 0-sample stop. The host then sees "I just got a pack".
    _pcm_vibrator_strobe = 0;

    uint8_t n = packet->count;
    if (n > 3)
    {
        n = 3;
    }

    if (n == 0)
    {
        // True stop: vibrationProcess adds nothing and does not reset
        // the sustain grain counter. The ring keeps playing.
    }
    else
    {
        // Non-zero unpack zeros the GetNextVibration counter (cuts the tail).
        _pcm_cut_hold = true;
        for (uint8_t i = 0; i < n; i++)
        {
            if (!pcm_amfm_push_pair_locked(&packet->pairs[i], i == 0))
            {
                break;
            }
        }
    }
    pcm_vibrator_refresh_nibble_locked();
    MUTEX_HAL_EXIT(&_pcm_amfm_mtx);
    return true;
}

volatile bool _pcm_init_done = false;
void pcm_init(int intensity) 
{
    intensity = (intensity > 255) ? 255 : intensity;
    static uint8_t this_intensity = 0;

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
    // PCM synthesis only backs the LRA HAL; pull the board's amplitude ceiling /
    // frequency floors from the LRA config the first time we run. Zero entries
    // keep the library defaults set above.
    static bool _cfg_loaded = false;
    if(!_cfg_loaded)
    {
        _cfg_loaded = true;
        const hoja_config_s *hcfg = hoja_config_get();
        if(hcfg)
        {
            if(hcfg->haptics.intensity_max    > 0.0f) _pcm_max_safe_ratio = hcfg->haptics.intensity_max;
            if(hcfg->haptics.intensity_min_lo > 0.0f) _pcm_param_min_lo   = hcfg->haptics.intensity_min_lo;
            if(hcfg->haptics.intensity_min_hi > 0.0f) _pcm_param_min_hi   = hcfg->haptics.intensity_min_hi;
        }
    }
#endif

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
    float max_scaled = ((float) PCM_WRAP_HALF_VAL * scaler * _pcm_max_safe_ratio);

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

    _pcm_generate_sine_table(scaler);
    _pcm_generate_all_tables();

    if(_pcm_init_done) return;
    _pcm_init_done = true;
    pcm_amfm_queue_init();
}

uint8_t *_external_sample_l;
uint8_t *_external_sample_r;
uint32_t _external_sample_remaining_l = 0;
uint32_t _external_sample_remaining_r = 0;
uint32_t _external_sample_size_l = 0;
uint32_t _external_sample_size_r = 0;

void pcm_play_bump(bool arg_right, bool arg_left)
{
    if(!haptic_config->haptic_triggers) return;

    // Resolve physical channel routing at runtime. channel_swap mirrors the old
    // HOJA_HAPTICS_CHAN_SWAP define: when set, arg_left/arg_right map straight to
    // the left/right motors; when clear, they are crossed.
    bool left, right;
    bool channel_b_enable = false;
#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
    // Channel routing only applies to the dual-channel LRA HAL.
    const hoja_config_s *hcfg = hoja_config_get();
    const bool channel_swap = hcfg ? hcfg->haptics.channel_swap : false;
    channel_b_enable = hcfg ? hcfg->haptics.channel_b_enable : false;
#else
    const bool channel_swap = false;
#endif

    if(channel_swap) { left = arg_left;  right = arg_right; }
    else             { left = arg_right; right = arg_left;  }

    // Single-channel boards fold both sides onto the one motor.
    if(!channel_b_enable)
    {
        if(left)       right = true;
        else if(right) left  = true;
    }

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

void pcm_erm_set(uint8_t intensity, bool brake)
{
    if(!intensity)
    {
        _pcm_erm_state.target_percent = 0.0f;
    }
    else
    {
        _pcm_erm_state.target_percent = (float)intensity / 255.0f;
        _pcm_erm_state.erm_active = true;
        MUTEX_HAL_ENTER_BLOCKING(&_pcm_amfm_mtx);
        pcm_amfm_flush_locked();
        MUTEX_HAL_EXIT(&_pcm_amfm_mtx);
    }

    _pcm_erm_state.apply_brake = brake;
}

static void pcm_latch_pair(const haptic_processed_s *v,
                           uint32_t *hi_freq_inc, uint32_t *lo_freq_inc,
                           uint32_t *hi_amp_scaler, uint32_t *lo_amp_scaler,
                           uint32_t *target_dc_offset)
{
    *hi_freq_inc = v->hi_frequency_increment;
    *lo_freq_inc = v->lo_frequency_increment;

    *hi_amp_scaler = (v->hi_amplitude_fixed) ?
        ((v->hi_amplitude_fixed * _hi_amp_scaler_fixed) >> PCM_AMPLITUDE_BIT_SCALE) + _hi_amp_scaler_fixed_min : 0;
    *lo_amp_scaler = (v->lo_amplitude_fixed) ?
        ((v->lo_amplitude_fixed * _lo_amp_scaler_fixed) >> PCM_AMPLITUDE_BIT_SCALE) + _lo_amp_scaler_fixed_min : 0;

    uint32_t hi_peak = (*hi_amp_scaler * PCM_WRAP_HALF_VAL) >> PCM_AMPLITUDE_BIT_SCALE;
    uint32_t lo_peak = (*lo_amp_scaler * PCM_WRAP_HALF_VAL) >> PCM_AMPLITUDE_BIT_SCALE;
    *target_dc_offset = (hi_peak + lo_peak);
}

static uint32_t pcm_peak_phase(void)
{
    return PCM_SINE_WRAPAROUND / 4u;
}

static void pcm_snap_opposite_peak(uint32_t *phase_hi, uint32_t *phase_lo,
                                   uint32_t hi_amp, uint32_t lo_amp)
{
    const uint32_t quad = PCM_SINE_WRAPAROUND / 4u;
    uint16_t idx_hi = (uint16_t)((*phase_hi >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE);
    uint16_t idx_lo = (uint16_t)((*phase_lo >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE);
    int16_t s = (hi_amp >= lo_amp) ? _pcm_sine_table[idx_hi] : _pcm_sine_table[idx_lo];
    uint32_t peak = (s >= 0) ? (quad * 3u) : quad;
    *phase_hi = peak;
    *phase_lo = peak;
}

// Mix one PWM half-buffer. Two synthesizers share the same sine mixer:
//   ERM:  one envelope step per buffer, continuous phase, no peak snap.
//   HD:   50-sample grains from the pair FIFO; first pair of a report snaps
//         to a peak. Empty queue holds the last pair, then 97/100 decay.
void pcm_generate_buffer(uint32_t *buffer)
{
    static uint32_t phase_hi = 0;
    static uint32_t phase_lo = 0;

    static haptic_processed_s current_pair = {0};
    static uint16_t samples_remaining = 0;
    static bool     is_active = false;
    static bool     is_holding = false;
    static bool     parked = true;
    static uint16_t hold_grains = 0;

    static uint32_t hi_freq_inc = 200;
    static uint32_t lo_freq_inc = 200;
    static uint32_t hi_amp_scaler = 0;
    static uint32_t lo_amp_scaler = 0;
    static uint32_t target_dc_offset = 0;
    static uint32_t dc_offset = 0;
    static bool erm_playing = false;

    if (pcm_erm_driving())
    {
        pcm_erm_handler(&_pcm_erm_state);
    }

    const bool erm_live = pcm_erm_driving();
    if (erm_live)
    {
        samples_remaining = 0;
    }
    else if (erm_playing)
    {
        is_active = false;
        is_holding = false;
        hold_grains = 0;
        memset(&current_pair, 0, sizeof(current_pair));
        hi_amp_scaler = 0;
        lo_amp_scaler = 0;
        target_dc_offset = 0;
        dc_offset = 0;
        samples_remaining = 0;
    }
    erm_playing = erm_live;

    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
        if (!erm_live && is_holding && _pcm_cut_hold)
        {
            samples_remaining = 0;
            is_holding = false;
        }

        if (samples_remaining == 0)
        {
            haptic_processed_s inbound = {0};
            bool note_on = false;
            bool got_pair = false;

            if (erm_live)
            {
                current_pair = _pcm_erm_pair;
                is_holding = false;
                hold_grains = 0;
                is_active = (pcm_pair_energy(&current_pair) != 0u);
            }
            else
            {
                got_pair = pcm_amfm_try_pop_pair(&inbound, &note_on);

                if (got_pair)
                {
                    current_pair = inbound;
                    is_holding = false;
                    hold_grains = 0;
                    _pcm_cut_hold = false;
                    is_active = (pcm_pair_energy(&current_pair) != 0u);
                }
                else if (is_active)
                {
                    is_holding = true;
                    hold_grains++;
                    if (hold_grains >= PCM_OFFICIAL_HOLD_GRAINS)
                    {
                        current_pair.hi_amplitude_fixed =
                            (uint16_t)((current_pair.hi_amplitude_fixed * 97u) / 100u);
                        current_pair.lo_amplitude_fixed =
                            (uint16_t)((current_pair.lo_amplitude_fixed * 97u) / 100u);
                        if (pcm_pair_energy(&current_pair) == 0u)
                        {
                            is_active = false;
                        }
                    }
                }
            }

            if (!is_active)
            {
                hi_amp_scaler = 0;
                lo_amp_scaler = 0;
                target_dc_offset = 0;
                if (got_pair)
                {
                    dc_offset = 0;
                    phase_hi = 0;
                    phase_lo = 0;
                    parked = true;
                    samples_remaining = PCM_SAMPLES_PER_GRAIN;
                }
                else
                {
                    samples_remaining = erm_live ? (uint16_t)PCM_BUFFER_SIZE : 1;
                }
            }
            else if (erm_live)
            {
                pcm_latch_pair(&current_pair, &hi_freq_inc, &lo_freq_inc,
                               &hi_amp_scaler, &lo_amp_scaler, &target_dc_offset);
                parked = false;
                samples_remaining = (uint16_t)PCM_BUFFER_SIZE;
            }
            else
            {
                const uint32_t prev_hi = hi_amp_scaler;
                const uint32_t prev_lo = lo_amp_scaler;
                pcm_latch_pair(&current_pair, &hi_freq_inc, &lo_freq_inc,
                               &hi_amp_scaler, &lo_amp_scaler, &target_dc_offset);

                if (got_pair && note_on)
                {
                    if (parked || (prev_hi == 0u && prev_lo == 0u))
                    {
                        phase_hi = pcm_peak_phase();
                        phase_lo = pcm_peak_phase();
                    }
                    else
                    {
                        pcm_snap_opposite_peak(&phase_hi, &phase_lo, prev_hi, prev_lo);
                    }
                    dc_offset = target_dc_offset;
                    parked = false;
                }

                samples_remaining = PCM_SAMPLES_PER_GRAIN;
            }
        }

        uint16_t idx_hi = (phase_hi >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;
        uint16_t idx_lo = (phase_lo >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;

        int16_t sine_hi = _pcm_sine_table[idx_hi];
        int16_t sine_lo = _pcm_sine_table[idx_lo];

        int32_t scaled_hi = ((int32_t)((sine_hi >= 0 ? sine_hi : -sine_hi) * hi_amp_scaler) >> PCM_AMPLITUDE_BIT_SCALE);
        int32_t scaled_lo = ((int32_t)((sine_lo >= 0 ? sine_lo : -sine_lo) * lo_amp_scaler) >> PCM_AMPLITUDE_BIT_SCALE);

        if (sine_hi < 0) scaled_hi = -scaled_hi;
        if (sine_lo < 0) scaled_lo = -scaled_lo;

        int32_t mixed = (scaled_hi + scaled_lo);

        if (dc_offset < target_dc_offset) dc_offset+=2;
        else if (dc_offset > target_dc_offset) dc_offset--;

        mixed += dc_offset;
        if (mixed < 0) mixed = 0;

        uint32_t ext_l = 0, ext_r = 0;
        if (_external_sample_remaining_l) {
            ext_l = (uint32_t)_external_sample_l[_external_sample_size_l - _external_sample_remaining_l] * _external_sample_scaler;
            _external_sample_remaining_l--;
        }
        if (_external_sample_remaining_r) {
            ext_r = (uint32_t)_external_sample_r[_external_sample_size_r - _external_sample_remaining_r] * _external_sample_scaler;
            _external_sample_remaining_r--;
        }

        static bool load_new_raw = false;
        int16_t pcm_raw_val = 0;
        if (load_new_raw && pcm_raw_queue_pop(&pcm_raw_val)) {
            if (pcm_raw_val >= 0) mixed = (pcm_raw_val >> 3);
        }
        load_new_raw = !load_new_raw;

        buffer[i] = (((uint32_t)(mixed + ext_l)) << 16) | ((uint32_t)(mixed + ext_r));

        if (hi_amp_scaler) phase_hi = (phase_hi + hi_freq_inc) % PCM_SINE_WRAPAROUND;
        else               phase_hi = 0;
        if (lo_amp_scaler) phase_lo = (phase_lo + lo_freq_inc) % PCM_SINE_WRAPAROUND;
        else               phase_lo = 0;

        if (samples_remaining > 0) samples_remaining--;
    }
}
