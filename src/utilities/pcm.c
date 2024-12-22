#include "utilities/pcm.h"
#include "hal/mutex_hal.h"

#include "switch/switch_fp_haptics.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "hoja_bsp.h"
#if HOJA_BSP_CHIPSET == CHIPSET_RP2040
    // Special float functions for RP2040
    #include "pico/float.h"
#endif

int16_t _pcm_sine_table[256];

// Initialize the sine table
void _generate_sine_table() {
    // Maximum amplitude for 16-bit signed integer
    const int16_t MAX_AMPLITUDE = 32767;
    
    // Generate 256 entries to cover a full sine wave cycle
    for (int i = 0; i < 256; i++) {
        // Convert index to angle in radians
        // We use 2π to cover a full cycle in 256 steps
        double angle = (2.0 * M_PI * i) / 256.0;
        
        // Calculate sine value and scale to int16_t range
        // sin() returns values between -1 and 1
        double scaled_sine = sin(angle) * MAX_AMPLITUDE;
        
        // Convert to int16_t, rounding to nearest value
        _pcm_sine_table[i] = (int16_t)round(scaled_sine);
    }
}

#define PCM_AMFM_QUEUE_SIZE 16  // Adjust size as needed

typedef struct {
    haptic_processed_s buffer[PCM_AMFM_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} pcm_amfm_queue_t;

pcm_amfm_queue_t _pcm_amfm_queue;

// Initialize the queue
void pcm_amfm_queue_init() {
    _pcm_amfm_queue.head = 0;
    _pcm_amfm_queue.tail = 0;
    _pcm_amfm_queue.count = 0;
}

// Push new values to queue
// Returns true if successful, false if queue is full
bool pcm_amfm_push(haptic_processed_s *values) {
    if (_pcm_amfm_queue.count >= PCM_AMFM_QUEUE_SIZE) {
        return false;  // Queue is full
    }
    
    _pcm_amfm_queue.buffer[_pcm_amfm_queue.tail] = *values;
    _pcm_amfm_queue.tail = (_pcm_amfm_queue.tail + 1) % PCM_AMFM_QUEUE_SIZE;
    _pcm_amfm_queue.count++;
    
    return true;
}

// Pop values from queue
// Returns true if successful, false if queue is empty
bool pcm_amfm_pop(haptic_processed_s *values) {
    if (_pcm_amfm_queue.count == 0) {
        return false;  // Queue is empty
    }
    
    *values = _pcm_amfm_queue.buffer[_pcm_amfm_queue.head];
    _pcm_amfm_queue.head = (_pcm_amfm_queue.head + 1) % PCM_AMFM_QUEUE_SIZE;
    _pcm_amfm_queue.count--;
    
    return true;
}

// Helper function to check if queue is empty
bool pcm_amfm_is_empty() {
    return _pcm_amfm_queue.count == 0;
}

// Helper function to check if queue is full
bool pcm_amfm_is_full() {
    return _pcm_amfm_queue.count >= PCM_AMFM_QUEUE_SIZE;
}

// For phase increments (Q8.8 format)
static inline uint16_t _lerp_phase_increment(uint16_t start, uint16_t end, uint8_t t) 
{
    uint32_t diff = end - start;
    return start + ((diff * t) >> 8);
}

// For amplitudes (8-bit values)
static inline uint8_t _lerp_amplitude(uint8_t start, uint8_t end, uint8_t t) 
{
    int16_t diff = end - start;  // Signed for negative differences
    return start + ((diff * t) >> 8);
}

// Generate 255 samples of PCM data
void pcm_generate_buffer(
    uint8_t *buffer          // Output buffer
) 
{
    // Local variables for phase accumulators
    static uint16_t phase_hi = 0;
    static uint16_t phase_lo = 0;
    static uint8_t current_sample_idx = 0;
    static haptic_processed_s current_values = {0};

    for(size_t i = 0; i < PCM_BUFFER_SIZE; i++) {

        if(current_sample_idx >= PCM_SAMPLES_PER_PAIR) 
        {
            current_sample_idx = 0;
        }

        if(!current_sample_idx) {
            if(!pcm_amfm_is_empty(&_pcm_amfm_queue)) {
                pcm_amfm_pop(&_pcm_amfm_queue, &current_values);
            }
        }

        int16_t sine_hi = sine_table[phase_hi >> 8];
        int16_t sine_lo = sine_table[phase_lo >> 8];
        
        // Process only positive half of sine wave
        // Scale amplitude and map to 0-255 range
        uint32_t scaled_hi = (sine_hi > 0) ? 
            ((uint32_t)sine_hi * (current_values.hi_amplitude_fixed >> 16)) : 0;
        uint32_t scaled_lo = (sine_lo > 0) ? 
            ((uint32_t)sine_lo * (current_values.lo_amplitude_fixed >> 16)) : 0;
        
        // Mix the two channels
        uint32_t mixed = scaled_hi + scaled_lo;
        
        // Clamp to 0-255 range
        mixed = (mixed > 255) ? 255 : mixed;
        
        buffer[i] = (uint8_t)mixed;
        
        phase_hi += current_values.hi_frequency_increment;
        phase_lo += current_values.lo_frequency_increment;

        current_sample_idx++;
    }
}
