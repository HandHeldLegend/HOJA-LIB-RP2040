#ifndef UTILITIES_PCM_H
#define UTILITIES_PCM_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"
#include "devices_shared_types.h"

#define PCM_BUFFER_SIZE 255
#define PCM_SAMPLES_PER_PAIR 60
#define PCM_TOTAL_SIZE (PCM_BUFFER_SIZE * PCM_CHUNKS)
#define PCM_SAMPLE_RATE 12000
#define PCM_SINE_TABLE_SIZE 512
#define PCM_SINE_TABLE_IDX_MAX (PCM_SINE_TABLE_SIZE - 1)

#define PCM_FIXED_POINT_SCALE_BITS 8  // How many bits for fraction
#define PCM_FIXED_POINT_SCALE_FREQ (1 << PCM_FIXED_POINT_SCALE_BITS)

bool pcm_amfm_push(haptic_processed_s *values);

#endif