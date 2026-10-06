#include "utilities/pcm.h"
#include "utilities/crosscore_utils.h"

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

// The official firmware synthesizes 20 samples per AmFm pair at 4 kHz
// (sub_2211B6 sets both oscillators to 4000 Hz), so one grain is 5 ms.
#define PCM_OFFICIAL_SAMPLE_RATE        4000
#define PCM_OFFICIAL_SAMPLES_PER_GRAIN  20
#define PCM_SAMPLES_PER_GRAIN           ((PCM_OFFICIAL_SAMPLES_PER_GRAIN * PCM_SAMPLE_RATE) / PCM_OFFICIAL_SAMPLE_RATE)

// Its software PCM ring holds 160 samples, i.e. 8 grains. vibrationProcess
// refuses a pair once fewer than 20 samples are free.
#define PCM_OFFICIAL_RING_SAMPLES       160
#define PCM_AMFM_QUEUE_SIZE             (PCM_OFFICIAL_RING_SAMPLES / PCM_OFFICIAL_SAMPLES_PER_GRAIN)

_Static_assert((PCM_AMFM_QUEUE_SIZE & (PCM_AMFM_QUEUE_SIZE - 1)) == 0,
               "HOJA_CROSSCORE_FIFO_TYPE needs a power-of-two length");

// When the ring runs dry, GetNextVibration tops it up with a grain of AmFm
// code 0x18 (no change) for this many grains, then code 0x1B (amplitude down
// one step of 2^(-1/32)) for every grain after that.
#define PCM_OFFICIAL_HOLD_GRAINS        201
#define PCM_HOLD_DECAY_Q15              32066u // 2^(-1/32) * 32768

// Oscillator frequencies before the first pair arrives (sub_2210C2)
#define PCM_DEFAULT_LO_FREQUENCY_HZ     160.0f
#define PCM_DEFAULT_HI_FREQUENCY_HZ     320.0f

// Lock-free SPSC ring of AmFm pairs. pcm_amfm_push() is the only producer and
// pcm_generate_buffer() on core 1 the only consumer. Rumble can arrive in IRQ
// context (WLAN delivers it from the cyw43 background IRQ), so nothing on this
// path may take a lock, and only the consumer may empty the ring.
HOJA_CROSSCORE_FIFO_TYPE(amfm, haptic_processed_s, PCM_AMFM_QUEUE_SIZE);
static hoja_fifo_amfm_t _pcm_amfm_fifo = {0};

// Latest SInput HD pair. It is played until replaced, not queued.
HOJA_CROSSCORE_SNAPSHOT_TYPE(pcm_direct, haptic_processed_s);
static hoja_snapshot_pcm_direct_t _pcm_direct = {0};

// Requests from the producer side, serviced by pcm_generate_buffer()
static volatile bool _pcm_amfm_flush_req  = false; // Drop queued HD pairs
static volatile bool _pcm_erm_cancel_req  = false; // Stop the ERM simulation now
static volatile bool _pcm_direct_drop_req = false; // Stop playing the SInput pair

// Motor-status nibble for the Switch input report. pcm_amfm_push() latches the
// fill level; core 1 sets the strobe whenever it tops up a dry ring, and a
// non-empty rumble word clears it.
static volatile uint8_t  _pcm_vibrator_fill = 0;
static volatile bool     _pcm_vibrator_strobe = false;
// Samples left in the grain core 1 is playing, for the fill level
static volatile uint16_t _pcm_grain_samples_left = 0;

static uint8_t _pcm_amfm_count(void)
{
    // A push and pop racing the read can briefly overshoot; clamp it.
    unsigned int count = hoja_fifo_amfm_count(&_pcm_amfm_fifo);
    return (uint8_t)((count > PCM_AMFM_QUEUE_SIZE) ? PCM_AMFM_QUEUE_SIZE : count);
}

// Consumer side only: empties the ring by advancing head.
static void _pcm_amfm_drain(void)
{
    haptic_processed_s discard;
    while (hoja_fifo_amfm_pop(&_pcm_amfm_fifo, &discard)) { }
}

static uint32_t _pcm_pair_energy(const haptic_processed_s *pair)
{
    return (uint32_t)pair->hi_amplitude_fixed + (uint32_t)pair->lo_amplitude_fixed;
}

// Ring fill as UnpackAmFmCodes computes it: official samples not yet played,
// divided by 5 and capped at 7.
static uint8_t _pcm_fill_level(void)
{
    uint32_t used = ((uint32_t)_pcm_amfm_count() * PCM_OFFICIAL_SAMPLES_PER_GRAIN) +
                    (((uint32_t)_pcm_grain_samples_left * PCM_OFFICIAL_SAMPLE_RATE) / PCM_SAMPLE_RATE);
    used /= 5u;
    return (uint8_t)((used > 7u) ? 7u : used);
}

// Motor-status nibble for the Switch input report: fill level in bits 0-2 and
// the strobe in bit 3.
uint8_t ns_api_hook_get_vibrator_nibble(void)
{
    return (uint8_t)(_pcm_vibrator_fill | (_pcm_vibrator_strobe ? 0x08u : 0u));
}

// Percent at or below which the simulated motor counts as stopped
#define PCM_ERM_STOPPED 0.001f

// Each field has one writer: target and brake belong to the producer side
// (pcm_erm_set and the HD entry points), current to core 1.
typedef struct
{
    float target_percent;
    float current_percent;
    bool  apply_brake;
} pcm_erm_state_s;

// Standard (non-HD) rumble on an LRA simulates an ERM motor. pcm_erm_set()
// only moves the target; pcm_generate_buffer() steps the envelope once per
// buffer and plays _pcm_erm_pair directly. This bypasses the AmFm ring on
// purpose: its 201-grain hold would turn a smooth motor ramp into a long tail.
static haptic_processed_s _pcm_erm_pair = {0};
static pcm_erm_state_s _pcm_erm_state = {
    .target_percent = 0.0f,
    .current_percent = 0.0f,
    .apply_brake = false,
};

// True while a request is live or the motor is still spinning down
static bool _pcm_erm_driving(void)
{
    return (_pcm_erm_state.target_percent > PCM_ERM_STOPPED) ||
           (_pcm_erm_state.current_percent > PCM_ERM_STOPPED);
}

// Producer side: another source has taken the LRA, so stop the motor at once
static void _pcm_erm_stop(void)
{
    _pcm_erm_state.target_percent = 0.0f;
    _pcm_erm_cancel_req = true;
}

// Simulation handler where the PCM samples represent ERM vibrations with
// natural motor characteristics. Uses unified percent control for realistic
// frequency/amplitude coupling.
static void _pcm_erm_handler(pcm_erm_state_s *state)
{
    // Time constants for exponential curves (smaller = faster response)
    const float time_constant_up   = 0.125f;  // Spin-up rate
    const float time_constant_down = 0.125f;  // Spin-down rate

    // Brake multiplier for faster deceleration
    const float brake_multiplier = 2.5f;

    // Frequency range mapping
    const float min_freq_lo = 40.0f;   // Minimum frequency when motor starts
    const float min_freq_hi = 40.0f;   // Minimum frequency when motor starts
    const float target_freq_lo = 85.0f;
    const float target_freq_hi = 170.0f;

    // Amplitude range mapping
    const float min_amp_lo = 0.0f;     // Zero amplitude when motor stops
    const float min_amp_hi = 0.0f;     // Zero amplitude when motor stops
    const float target_amp_lo = 0.1f;
    const float target_amp_hi = 0.1f;

    // Unified motor control with exponential curves
    float percent_diff = state->target_percent - state->current_percent;
    if (fabsf(percent_diff) > 0.001f) // Avoid unnecessary computation for tiny differences
    {
        float time_constant;
        if (percent_diff > 0.0f)
        {
            // Motor spinning up - overcomes inertia
            time_constant = time_constant_up;
        }
        else
        {
            // Motor spinning down - friction helps, brake can help more
            time_constant = time_constant_down;
            if (state->apply_brake)
            {
                time_constant *= brake_multiplier;
            }
        }

        // Exponential approach: current += (target - current) * time_constant
        state->current_percent += percent_diff * time_constant;

        // Clamp to target when very close (prevents oscillation)
        if (fabsf(state->target_percent - state->current_percent) < 0.005f)
        {
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

    _pcm_erm_pair.hi_frequency_increment = pcm_frequency_to_fixedpoint_increment(actual_freq_hi);
    _pcm_erm_pair.lo_frequency_increment = pcm_frequency_to_fixedpoint_increment(actual_freq_lo);
    _pcm_erm_pair.hi_amplitude_fixed = pcm_amplitude_to_fixedpoint(actual_amp_hi);
    _pcm_erm_pair.lo_amplitude_fixed = pcm_amplitude_to_fixedpoint(actual_amp_lo);

    // Motor stops once it has spun down and nothing is asking for more
    if ((state->current_percent <= PCM_ERM_STOPPED) && (state->target_percent <= PCM_ERM_STOPPED))
    {
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

// Fractional bits kept on amplitude and frequency so per-sample ramp steps
// are not truncated away
#define PCM_RAMP_FRAC_BITS 8

// One oscillator band. Like the official sub_221250, amplitude and frequency
// slide linearly to their targets across each grain, and phase runs on
// continuously except after a grain that ends silent.
typedef struct
{
    uint32_t phase;
    int32_t  amp;        // Amplitude scaler, PCM_RAMP_FRAC_BITS fraction
    int32_t  inc;        // Phase increment, PCM_RAMP_FRAC_BITS fraction
    int32_t  amp_step;
    int32_t  inc_step;
    int32_t  amp_target;
    int32_t  inc_target;
} pcm_band_s;

// Oscillator and grain state for pcm_generate_buffer()
typedef struct
{
    pcm_band_s hi;
    pcm_band_s lo;

    haptic_processed_s pair;          // Last ring pair, replayed while the ring is dry
    uint16_t samples_remaining;       // Samples left in the current grain
    uint16_t hold_grains;             // Grains the last pair has been replayed for
    bool     direct_playing;          // Previous buffer played a non-silent SInput pair
    unsigned int direct_ignore_seq;   // SInput cell sequence when it was last dropped
} pcm_synth_s;

static pcm_synth_s _pcm_synth = {0};

static void _pcm_synth_reset(void)
{
    pcm_synth_s *s = &_pcm_synth;

    _pcm_amfm_drain();
    _pcm_vibrator_strobe = false;

    memset(s, 0, sizeof(*s));
    s->lo.inc = (int32_t)pcm_frequency_to_fixedpoint_increment(PCM_DEFAULT_LO_FREQUENCY_HZ) << PCM_RAMP_FRAC_BITS;
    s->hi.inc = (int32_t)pcm_frequency_to_fixedpoint_increment(PCM_DEFAULT_HI_FREQUENCY_HZ) << PCM_RAMP_FRAC_BITS;
    s->lo.inc_target = s->lo.inc;
    s->hi.inc_target = s->hi.inc;
}

// Board and intensity scaling of a pair amplitude (scaling logic from oldpcm)
static uint32_t _pcm_amp_scaler(uint16_t amplitude, uint32_t scaler, uint32_t scaler_min)
{
    return amplitude ? (((uint32_t)amplitude * scaler) >> PCM_AMPLITUDE_BIT_SCALE) + scaler_min : 0;
}

static void _pcm_band_begin(pcm_band_s *b, uint32_t amp, uint32_t inc, uint16_t len)
{
    b->amp_target = (int32_t)(amp << PCM_RAMP_FRAC_BITS);
    b->inc_target = (int32_t)(inc << PCM_RAMP_FRAC_BITS);
    b->amp_step = (b->amp_target - b->amp) / (int32_t)len;
    b->inc_step = (b->inc_target - b->inc) / (int32_t)len;
}

static void _pcm_band_end(pcm_band_s *b)
{
    b->amp = b->amp_target;
    b->inc = b->inc_target;

    // A band that faded out restarts from its zero crossing
    if (!b->amp)
    {
        b->phase = 0;
    }
}

// Step the ramps, advance the phase, then render one signed sample
static int32_t _pcm_band_render(pcm_band_s *b)
{
    b->amp += b->amp_step;
    b->inc += b->inc_step;
    b->phase = (b->phase + ((uint32_t)b->inc >> PCM_RAMP_FRAC_BITS)) % PCM_SINE_WRAPAROUND;

    int32_t sine = _pcm_sine_table[(b->phase >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE];
    return (sine * (b->amp >> PCM_RAMP_FRAC_BITS)) / PCM_AMPLITUDE_SHIFT_FIXED;
}

// Start a grain of len samples that slides from the current sound to pair
static void _pcm_synth_begin_grain(pcm_synth_s *s, const haptic_processed_s *pair, uint16_t len)
{
    _pcm_band_begin(&s->hi,
                    _pcm_amp_scaler(pair->hi_amplitude_fixed, _hi_amp_scaler_fixed, _hi_amp_scaler_fixed_min),
                    pair->hi_frequency_increment, len);
    _pcm_band_begin(&s->lo,
                    _pcm_amp_scaler(pair->lo_amplitude_fixed, _lo_amp_scaler_fixed, _lo_amp_scaler_fixed_min),
                    pair->lo_frequency_increment, len);
    s->samples_remaining = len;
}

static void _pcm_synth_end_grain(pcm_synth_s *s)
{
    _pcm_band_end(&s->hi);
    _pcm_band_end(&s->lo);
}

// Start the next grain from the ring. With the ring dry, top it up the way
// GetNextVibration does: replay the last pair, then decay it.
static void _pcm_synth_next_ring_grain(pcm_synth_s *s)
{
    if (hoja_fifo_amfm_pop(&_pcm_amfm_fifo, &s->pair))
    {
        s->hold_grains = 0;
    }
    else
    {
        _pcm_vibrator_strobe = true;

        if (s->hold_grains < PCM_OFFICIAL_HOLD_GRAINS)
        {
            s->hold_grains++;
        }
        else
        {
            s->pair.hi_amplitude_fixed = (uint16_t)(((uint32_t)s->pair.hi_amplitude_fixed * PCM_HOLD_DECAY_Q15) >> 15);
            s->pair.lo_amplitude_fixed = (uint16_t)(((uint32_t)s->pair.lo_amplitude_fixed * PCM_HOLD_DECAY_Q15) >> 15);
        }
    }

    _pcm_synth_begin_grain(s, &s->pair, PCM_SAMPLES_PER_GRAIN);
}

// Render one sample of the two-band oscillator
static int32_t _pcm_synth_render(pcm_synth_s *s)
{
    int32_t mixed = _pcm_band_render(&s->hi) + _pcm_band_render(&s->lo);

    // The PWM output is unipolar, so lift the waveform by its own envelope
    uint32_t envelope = ((uint32_t)s->hi.amp >> PCM_RAMP_FRAC_BITS) + ((uint32_t)s->lo.amp >> PCM_RAMP_FRAC_BITS);
    mixed += (int32_t)((envelope * PCM_WRAP_HALF_VAL) >> PCM_AMPLITUDE_BIT_SCALE);

    return (mixed < 0) ? 0 : mixed;
}

// Act on requests the producer side can't carry out itself
static void _pcm_service_requests(pcm_synth_s *s)
{
    if (_pcm_amfm_flush_req)
    {
        _pcm_amfm_flush_req = false;
        _pcm_amfm_drain();
    }

    if (_pcm_erm_cancel_req)
    {
        _pcm_erm_cancel_req = false;
        _pcm_erm_state.current_percent = 0.0f;
        memset(&_pcm_erm_pair, 0, sizeof(_pcm_erm_pair));
    }

    if (_pcm_direct_drop_req)
    {
        // Ignore the SInput cell until it is written again
        _pcm_direct_drop_req = false;
        s->direct_ignore_seq = atomic_load_explicit(&_pcm_direct.seq, memory_order_acquire);
        s->direct_playing = false;
    }
}

// The latest SInput pair, plus one silent grain after it stops so it fades out
static bool _pcm_direct_pair(pcm_synth_s *s, haptic_processed_s *out)
{
    bool live = false;

    if (atomic_load_explicit(&_pcm_direct.seq, memory_order_acquire) != s->direct_ignore_seq)
    {
        // A read racing a write returns the previous pair, which is fine here
        hoja_snapshot_pcm_direct_read(&_pcm_direct, out);
        live = (_pcm_pair_energy(out) != 0u);
    }

    if (!live)
    {
        memset(out, 0, sizeof(*out));
    }

    bool play = live || s->direct_playing;
    s->direct_playing = live;
    return play;
}

// Pair for a source that plays continuously instead of through the ring:
// the ERM simulation, else SInput HD. False when the ring should play.
static bool _pcm_continuous_pair(pcm_synth_s *s, haptic_processed_s *out)
{
    if (_pcm_erm_driving())
    {
        *out = _pcm_erm_pair;
        return true;
    }
    return _pcm_direct_pair(s, out);
}

bool pcm_amfm_push(haptic_packet_s *packet)
{
    if (!packet)
    {
        return false;
    }

    // HD rumble owns the LRA from here on
    _pcm_erm_stop();

    // UnpackAmFmCodes latches the fill level on every word, before queueing
    _pcm_vibrator_fill = _pcm_fill_level();

    // An empty (stop) word adds nothing and leaves the strobe and any held
    // pair alone
    uint8_t count = (packet->count > 3) ? 3 : packet->count;
    if (count == 0)
    {
        return true;
    }

    _pcm_vibrator_strobe = false;
    for (uint8_t i = 0; i < count; i++)
    {
        // Like vibrationProcess, a full ring drops this pair and the rest of
        // the report.
        if (!hoja_fifo_amfm_push(&_pcm_amfm_fifo, &packet->pairs[i]))
        {
            break;
        }
    }
    return true;
}

void pcm_direct_set(const haptic_processed_s *pair)
{
    if (!pair)
    {
        return;
    }

    // SInput owns the LRA from here on
    _pcm_erm_stop();
    _pcm_amfm_flush_req = true;
    hoja_snapshot_pcm_direct_write(&_pcm_direct, pair);
}

void pcm_erm_set(uint8_t intensity, bool brake)
{
    // Zero lets the motor spin down; the envelope stops itself
    _pcm_erm_state.target_percent = (float)intensity / 255.0f;
    _pcm_erm_state.apply_brake = brake;

    if (intensity)
    {
        // Drop queued HD pairs and any SInput pair so they don't resume once
        // the motor stops. Only core 1 may touch those, so it does it on its
        // next buffer.
        _pcm_amfm_flush_req = true;
        _pcm_direct_drop_req = true;
    }
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
    _pcm_synth_reset();
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

// One-pole low-pass on the outgoing signal: y += (x - y) >> shift per sample.
// At 8 kHz, shift 1 is about 880 Hz and shift 2 about 370 Hz; 0 disables it.
#define PCM_OUTPUT_SMOOTH_SHIFT 1
static int32_t _pcm_output_smoothed = 0; // 8 fractional bits

// Fill one PWM half-buffer. The oscillator plays either a continuous source
// (the ERM simulation or the latest SInput pair, one grain per buffer) or HD
// grains from the AmFm ring; trigger bumps and raw PCM are mixed on top.
void pcm_generate_buffer(uint32_t *buffer)
{
    pcm_synth_s *s = &_pcm_synth;

    _pcm_service_requests(s);

    if (_pcm_erm_driving())
    {
        _pcm_erm_handler(&_pcm_erm_state);
    }

    haptic_processed_s continuous;
    if (_pcm_continuous_pair(s, &continuous))
    {
        // Slide to the new pair across the whole buffer, and forget the ring's
        // held pair so it can't resume once this source stops
        memset(&s->pair, 0, sizeof(s->pair));
        _pcm_synth_begin_grain(s, &continuous, PCM_BUFFER_SIZE);
    }

    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
        if (s->samples_remaining == 0)
        {
            _pcm_synth_next_ring_grain(s);
        }

        int32_t mixed = _pcm_synth_render(s);

        if (--s->samples_remaining == 0)
        {
            _pcm_synth_end_grain(s);
        }

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

        // Round off sharp edges before the trigger bumps, which should stay crisp
        _pcm_output_smoothed += ((mixed << 8) - _pcm_output_smoothed) >> PCM_OUTPUT_SMOOTH_SHIFT;
        mixed = _pcm_output_smoothed >> 8;

        // Final Output
        buffer[i] = (((uint32_t)(mixed + ext_l)) << 16) | ((uint32_t)(mixed + ext_r));
    }

    _pcm_grain_samples_left = s->samples_remaining;
}
