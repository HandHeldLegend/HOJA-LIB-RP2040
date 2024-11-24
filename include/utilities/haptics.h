#ifndef HAPTICS_H
#define HAPTICS_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_types.h"

typedef struct
{
    float hi_freq_linear;
    float hi_amp_linear;
    float lo_freq_linear;
    float lo_amp_linear;
} __attribute__((packed)) hoja_haptic_frame_linear_s;

typedef struct
{
    float   high_frequency;
    float   high_amplitude;
    float   low_frequency;
    float   low_amplitude;
} __attribute__ ((packed)) hoja_haptic_frame_s;

typedef struct
{
    uint8_t sample_count;
    bool unread;
    hoja_haptic_frame_linear_s linear; // Last known state 
    hoja_haptic_frame_s samples[3];
} __attribute__ ((packed)) hoja_rumble_msg_s;

typedef struct
{
    float a_hi;
    float a_lo;
    float f_hi;
    float f_lo;
} amfm_s;

uint16_t haptics_mix_pcm(uint16_t pcm_a, uint16_t pcm_b);

bool haptics_set_all(float f_hi, float a_hi, float f_lo, float a_lo);

int8_t haptics_get(bool left, amfm_s* left_out, bool right, amfm_s* right_out);
bool haptics_set(const amfm_s* l_in, int8_t l_count, const amfm_s* r_in, int8_t r_count);

#endif