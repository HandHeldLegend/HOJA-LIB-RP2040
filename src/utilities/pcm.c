#include "utilities/pcm.h"
#include "hal/mutex_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

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

// Initialize the queue
void pcm_amfm_queue_init()
{
    _pcm_amfm_queue.head = 0;
    _pcm_amfm_queue.tail = 0;
    _pcm_amfm_queue.count = 0;
}

MUTEX_HAL_INIT(_pcm_amfm_mutex);

// Push new values to queue
// Returns true if successful, false if queue is full
bool pcm_amfm_push(haptic_processed_s *value)
{
    if (_pcm_amfm_queue.count >= PCM_AMFM_QUEUE_SIZE)
    {
        return false; // Queue is full
    }
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
    const  uint16_t lerp_inc = 225 / PCM_SAMPLES_LERP_TIME;

    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
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
        uint16_t hi_frequency_increment = 0;
        uint16_t lo_frequency_increment = 0;

        // Lerp between last and current values
        if (current_sample_idx < PCM_SAMPLES_LERP_TIME)
        {
            lerp_factor += lerp_inc;
            hi_amplitude_fixed = lerp_fixed_signed(last_values.hi_amplitude_fixed, current_values.hi_amplitude_fixed, lerp_factor);
            lo_amplitude_fixed = lerp_fixed_signed(last_values.lo_amplitude_fixed, current_values.lo_amplitude_fixed, lerp_factor);
            hi_frequency_increment = lerp_fixed_unsigned(last_values.hi_frequency_increment, current_values.hi_frequency_increment, lerp_factor);
            lo_frequency_increment = lerp_fixed_unsigned(last_values.lo_frequency_increment, current_values.lo_frequency_increment, lerp_factor);
        }
        else 
        {
            hi_amplitude_fixed = current_values.hi_amplitude_fixed;
            lo_amplitude_fixed = current_values.lo_amplitude_fixed;
            hi_frequency_increment = current_values.hi_frequency_increment;
            lo_frequency_increment = current_values.lo_frequency_increment;
        }

        uint16_t idx_hi = (phase_hi >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;
        uint16_t idx_lo = (phase_lo >> PCM_FREQUENCY_SHIFT_BITS) % PCM_SINE_TABLE_SIZE;

        int16_t sine_hi = _pcm_sine_table[idx_hi];
        int16_t sine_lo = _pcm_sine_table[idx_lo];

        // Using only positive half of sine wave
        uint32_t scaled_hi = (sine_hi > 0) ? ((uint32_t)sine_hi * (uint32_t)hi_amplitude_fixed) >> PCM_AMPLITUDE_BIT_SCALE : 0;
        uint32_t scaled_lo = (sine_lo > 0) ? ((uint32_t)sine_lo * (uint32_t)lo_amplitude_fixed) >> PCM_AMPLITUDE_BIT_SCALE : 0;

        scaled_hi >>= 7; // Scale down to 8-bit
        scaled_lo >>= 7; // Scale down to 8-bit

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
        if(lerp_factor<255) lerp_factor++;
    }
}
