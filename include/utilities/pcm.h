#ifndef UTILITIES_PCM_H
#define UTILITIES_PCM_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"
#include "devices_shared_types.h"

#include "board_config.h"

#define PCM_BUFFER_SIZE 256
#define PCM_SAMPLES_PER_PAIR 85
#define PCM_SAMPLES_LERP_TIME 0
#define PCM_TOTAL_SIZE (PCM_BUFFER_SIZE * PCM_CHUNKS)
#define PCM_SAMPLE_RATE 16000
#define PCM_SINE_TABLE_SIZE 4096

#if defined(BOARD_SAFE_PCM_MAX)
    #define PCM_MAX_SAFE_VALUE BOARD_SAFE_PCM_MAX
#else
    #define PCM_MAX_SAFE_VALUE 80 // 60 maybe felt closest to OEM?
#endif

#if defined(BOARD_LO_FREQUENCY_MIN)
    #define PCM_LO_FREQUENCY_MIN BOARD_LO_FREQUENCY_MIN
#else 
    #define PCM_LO_FREQUENCY_MIN 0.175f
#endif

#if defined(BOARD_HI_FREQUENCY_MIN)
    #define PCM_HI_FREQUENCY_MIN BOARD_HI_FREQUENCY_MIN
#else 
    #define PCM_HI_FREQUENCY_MIN 0.17f
#endif

#define PCM_LO_FREQUENCY_RANGE (1 - PCM_LO_FREQUENCY_MIN)
#define PCM_HI_FREQUENCY_RANGE (1 - PCM_HI_FREQUENCY_MIN)

#define PCM_SIN_RANGE_MAX 32767
#define PCM_SINE_TABLE_IDX_MAX (PCM_SINE_TABLE_SIZE - 1)

#define PCM_AMPLITUDE_BIT_SCALE 15
#define PCM_AMPLITUDE_SHIFT_FIXED (1 << PCM_AMPLITUDE_BIT_SCALE)

#define PCM_FREQUENCY_SHIFT_BITS 7 // How many bits for fraction
#define PCM_FREQUENCY_SHIFT_FIXED (1 << PCM_FREQUENCY_SHIFT_BITS)
#define PCM_SINE_WRAPAROUND (PCM_SINE_TABLE_SIZE << PCM_FREQUENCY_SHIFT_BITS)

void pcm_init(uint8_t intensity);
void pcm_play_sample(uint8_t *sample, uint32_t size);
void pcm_send_pulse();
bool pcm_amfm_push(haptic_processed_s *value);
void pcm_generate_buffer(uint32_t *buffer);

#endif