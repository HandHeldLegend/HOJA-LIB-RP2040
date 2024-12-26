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
#define TWO_PI 2.0f * M_PI

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
        _pcm_sine_table[i] = (int16_t)(sample * (float) PCM_SIN_RANGE_MAX);

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

void xpcm_set_frequency_amplitude(float frequency, float amplitude) 
{
    if(amplitude < 0) amplitude = 0;
    if(amplitude > 1) amplitude = 1;

    // Calculate phase increment float
    float increment = (frequency * PCM_SINE_TABLE_SIZE) / PCM_SAMPLE_RATE; 
    uint16_t fixed_increment = (uint16_t)(increment * PCM_FREQUENCY_SHIFT_FIXED + 0.5); 
    float scaled_amplitude = 0;

    if(amplitude > 0)
        scaled_amplitude = (amplitude * PCM_LO_FREQUENCY_RANGE) + PCM_LO_FREQUENCY_MIN;

    // Scale to fixed point
    int16_t  fixed_amplitude = (int16_t)(amplitude * PCM_AMPLITUDE_SHIFT_FIXED + 0.5);

}

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

void pcm_send_pulse()
{
    uint8_t pulse_data[4] = {0x00, 0x00, 0x05, 0xC0};
    switch_haptics_rumble_translate(pulse_data);
    switch_haptics_rumble_translate(pulse_data);
    switch_haptics_rumble_translate(pulse_data);
    switch_haptics_rumble_translate(pulse_data);
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

void pcm_init() 
{
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

// Generate 255 samples of PCM data
void pcm_generate_buffer(
    uint32_t *buffer // Output buffer
)
{
    // Local variables for phase accumulators
    static uint32_t phase_hi = 0;
    static uint32_t phase_lo = 0;
    static uint16_t current_sample_idx = 0;
    static haptic_processed_s   last_values     = {0};
    static haptic_processed_s   current_values  = {0};

    // Interpolation factor (t) goes from 0 to 255
    static uint8_t lerp_factor = 0; 

    static int load_sample = -1;

    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
        uint16_t idx_hi = (phase_hi >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;
        uint16_t idx_lo = (phase_lo >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;

        if(!current_sample_idx || (current_sample_idx >= PCM_SAMPLES_PER_PAIR)) 
        {
            // Get new values from queue
            if (!pcm_amfm_is_empty())
            {
                last_values = current_values;
                lerp_factor = 0;
                current_sample_idx = 0;
                pcm_amfm_pop(&current_values);
            }
        }

        int16_t hi_amplitude_fixed = 0;
        int16_t lo_amplitude_fixed = 0;
        uint16_t hi_frequency_increment = 200;
        uint16_t lo_frequency_increment = 200;

        hi_amplitude_fixed = current_values.hi_amplitude_fixed;
        lo_amplitude_fixed = current_values.lo_amplitude_fixed;
        hi_frequency_increment = current_values.hi_frequency_increment;
        lo_frequency_increment = current_values.lo_frequency_increment;

        int16_t sine_hi = _pcm_sine_table[idx_hi];
        int16_t sine_lo = _pcm_sine_table[idx_lo];

        // Using only positive half of sine wave
        uint32_t scaled_hi = (sine_hi > 0) ? ((uint32_t)sine_hi * (uint32_t)hi_amplitude_fixed) >> PCM_AMPLITUDE_BIT_SCALE : 0;
        uint32_t scaled_lo = (sine_lo > 0) ? ((uint32_t)sine_lo * (uint32_t)lo_amplitude_fixed) >> PCM_AMPLITUDE_BIT_SCALE : 0;

        /*
           High frequency is used for Special hit effects, tilt attack hit, tilt whiff,
           Smash attack chargeup, enemy death confirmation, Shield, Landing
        */
        scaled_hi >>= 7; // Scale down to 8-bit

        /*
           Low frequency is used for Jump, Smash attack, 
           Attack hit, Tilt IMPACT, Dash, Walk, Special, Shield, Air dodge
        */
        scaled_lo >>= 7; // Scale down to 8-bit

        // Debug disable lo 
        //scaled_lo = 0;

        // Debug disable hi
        //scaled_hi = 0;

        // Mix the two channels
        uint32_t    mixed = (scaled_hi + scaled_lo);

        #define RATIO_SHIFT (14)
        #define RATIO_MULT  (1 << RATIO_SHIFT)
        #define RATIO_FACTOR (RATIO_MULT * PCM_MAX_SAFE_VALUE)

        // Check if scaling is required
        if (mixed > PCM_MAX_SAFE_VALUE)
        {
            // Compute the scaling factor as a fixed-point ratio
            uint32_t new_ratio = RATIO_FACTOR / mixed;

            // Scale the components proportionally
            scaled_hi = (scaled_hi * new_ratio) >> RATIO_SHIFT;
            scaled_lo = (scaled_lo * new_ratio) >> RATIO_SHIFT;

            // Recalculate the mixed value (optional for verification)
            mixed = scaled_hi + scaled_lo;
        }

        uint8_t sample = (uint8_t)(mixed > PCM_MAX_SAFE_VALUE ? PCM_MAX_SAFE_VALUE : mixed);

        uint32_t outsample = ((uint32_t) sample << 16) | (uint8_t) sample;
        buffer[i] = outsample;

        phase_hi = (phase_hi + hi_frequency_increment) % PCM_SINE_WRAPAROUND;
        phase_lo = (phase_lo + lo_frequency_increment) % PCM_SINE_WRAPAROUND;

        current_sample_idx++;
    }
}
