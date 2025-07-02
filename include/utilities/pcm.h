#ifndef UTILITIES_PCM_H
#define UTILITIES_PCM_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"
#include "devices_shared_types.h"

#include "board_config.h"

#define PCM_RAW_QUEUE_SIZE 1024 // Adjust size as needed

#define PCM_BUFFER_SIZE         256
#define PCM_SAMPLES_PER_PAIR    120
#define PCM_SAMPLE_CHUNK_3      (PCM_SAMPLES_PER_PAIR / 3)
#define PCM_SAMPLE_CHUNK_2      (PCM_SAMPLES_PER_PAIR / 2)
#define PCM_WRAP_VAL            2770 //4096 

#define PCM_WRAP_HALF_VAL       (PCM_WRAP_VAL/2)
#define PCM_SAMPLE_REPETITIONS  3 //2

#define PCM_SAMPLE_RATE         16000
#define PCM_SINE_TABLE_SIZE     4096

#if defined(HOJA_HAPTICS_MAX)
    #define PCM_MAX_SAFE_RATIO      HOJA_HAPTICS_MAX
#else 
    #define PCM_MAX_SAFE_RATIO      0.285f
#endif

#if defined(HOJA_HAPTICS_MIN_LO)
    #define PCM_LO_FREQUENCY_MIN    HOJA_HAPTICS_MIN_LO
#else 
    #define PCM_LO_FREQUENCY_MIN    0.0625f
#endif

#if defined(HOJA_HAPTICS_MIN_HI)
    #define PCM_HI_FREQUENCY_MIN    HOJA_HAPTICS_MIN_HI
#else 
    #define PCM_HI_FREQUENCY_MIN    0.005f
#endif

#define PCM_SIN_RANGE_MAX 32767
#define PCM_SINE_TABLE_IDX_MAX (PCM_SINE_TABLE_SIZE - 1)

#define PCM_AMPLITUDE_BIT_SCALE 15
#define PCM_AMPLITUDE_SHIFT_FIXED (1 << PCM_AMPLITUDE_BIT_SCALE)

#define PCM_FREQUENCY_SHIFT_BITS 7 // How many bits for fraction
#define PCM_FREQUENCY_SHIFT_FIXED (1 << PCM_FREQUENCY_SHIFT_BITS)
#define PCM_SINE_WRAPAROUND (PCM_SINE_TABLE_SIZE << PCM_FREQUENCY_SHIFT_BITS)

typedef enum 
{
    PCM_DEBUG_PARAM_MAX,
    PCM_DEBUG_PARAM_MIN_LO, 
    PCM_DEBUG_PARAM_MIN_HI, 
} pcm_adjust_t;

void pcm_debug_adjust_param(uint8_t param_type, float amount);

int16_t pcm_raw_queue_count();
int16_t pcm_raw_queue_push(int16_t *data, uint16_t len);

void pcm_erm_set(uint8_t intensity, bool brake);

// PCM UTILITIES FOR EASIER USE
// Returns a Q8.8 fixed point increment value for a given frequency
uint16_t pcm_frequency_to_fixedpoint_increment(float frequency);
uint16_t pcm_amplitude_to_fixedpoint(float amplitude);
void pcm_init(int intensity);
void pcm_play_sample(uint8_t *sample, uint32_t size);
void pcm_send_pulse();
bool pcm_amfm_push(haptic_processed_s *value);
void pcm_generate_buffer(uint32_t *buffer);

#endif