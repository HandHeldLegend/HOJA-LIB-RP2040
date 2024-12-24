#ifndef UTILITIES_PCM_H
#define UTILITIES_PCM_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"
#include "devices_shared_types.h"

#define PCM_BUFFER_SIZE 255
#define PCM_SAMPLES_PER_PAIR 64
#define PCM_SAMPLES_LERP_TIME 48
#define PCM_TOTAL_SIZE (PCM_BUFFER_SIZE * PCM_CHUNKS)
#define PCM_SAMPLE_RATE 12000
#define PCM_SINE_TABLE_SIZE 4096
#define PCM_MAX_SAFE_VALUE 100

#define PCM_SIN_RANGE_MAX 32767
#define PCM_SINE_TABLE_IDX_MAX (PCM_SINE_TABLE_SIZE - 1)

#define PCM_AMPLITUDE_BIT_SCALE 7
#define PCM_AMPLITUDE_SHIFT_FIXED (1 << PCM_AMPLITUDE_BIT_SCALE)

#define PCM_FREQUENCY_SHIFT_BITS 7 // How many bits for fraction
#define PCM_FREQUENCY_SHIFT_FIXED (1 << PCM_FREQUENCY_SHIFT_BITS)
#define PCM_SINE_WRAPAROUND (PCM_SINE_TABLE_SIZE << PCM_FREQUENCY_SHIFT_BITS)

void pcm_init();
bool pcm_amfm_push(haptic_processed_s *value);
void pcm_generate_buffer(uint32_t *buffer);

#endif