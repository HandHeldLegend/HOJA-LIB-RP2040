#ifndef UTILITIES_PCM_H
#define UTILITIES_PCM_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"
#include "devices_shared_types.h"

#include "board_config.h"
#include "ns_lib.h"

#define PCM_RAW_QUEUE_SIZE 1024 // Adjust size as needed

#define PCM_BUFFER_SIZE         64
// Official firmware writes 0x14 software PCM samples per AmFm pair (sub_221394).
// Those samples run at ~3.2 kHz (timer2 1.25 ms * 4 samples in PlayVibration),
// so one grain is ~6.25 ms. At our 8 kHz PWM that is 50 samples, not 20.
#define PCM_SAMPLES_PER_GRAIN   50

#define PCM_WRAP_VAL            4096
#define PCM_WRAP_HALF_VAL       (PCM_WRAP_VAL / 2)

#define PCM_SAMPLE_REPETITIONS  4

#define PCM_SAMPLE_RATE         8000
#define PCM_SINE_TABLE_SIZE     4096

// Library defaults for the amplitude ceiling / frequency floors. A board can
// override any of these at runtime via hoja_config_s.haptics.intensity_* (0.0f
// keeps the default below).
#define PCM_MAX_SAFE_RATIO      0.285f
#define PCM_LO_FREQUENCY_MIN    0.0625f
#define PCM_HI_FREQUENCY_MIN    0.005f

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

void pcm_ns_to_fp(ns_haptics_packet_raw_s *in, haptic_packet_s *out);

int16_t pcm_raw_queue_count();
int16_t pcm_raw_queue_push(int16_t *data, uint16_t len);

// Standard rumble (Xbox / Sinput / Switch non-HD). On LRA boards this is a
// dual-sine motor sim. Do not send this through pcm_amfm_push.
void pcm_erm_set(uint8_t intensity, bool brake);

// PCM UTILITIES FOR EASIER USE
// Returns a Q8.8 fixed point increment value for a given frequency
uint16_t pcm_frequency_to_fixedpoint_increment(float frequency);
uint16_t pcm_amplitude_to_fixedpoint(float amplitude);
void pcm_init(int intensity);

// Trigger-feedback "bump". Called as pcm_play_bump(right_state, left_state).
// Channel routing (which physical motor is left/right) and the single-channel
// fold-down are resolved at runtime from hoja_config_s.haptics.
void pcm_play_bump(bool arg_right, bool arg_left);

void pcm_send_pulse();
// Switch HD rumble AM/FM pairs. Cancels any in-flight ERM simulation so the
// pair FIFO / grain playback owns the LRA.
bool pcm_amfm_push(haptic_packet_s *packet);
void pcm_generate_buffer(uint32_t *buffer);

#endif