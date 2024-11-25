#ifndef HOJA_HAPTICS_H
#define HOJA_HAPTICS_H

#include <stdint.h>
#include <stdbool.h>

#define HOJA_HAPTIC_BASE_HFREQ 320
#define HOJA_HAPTIC_BASE_LFREQ 130
#define HOJA_HAPTIC_BASE_AMP 0.55f

typedef struct
{
    float hi_freq_linear;
    float hi_amp_linear;
    float lo_freq_linear;
    float lo_amp_linear;
} hoja_haptic_frame_linear_s;

typedef struct
{
    float   high_frequency;
    float   high_amplitude;
    float   low_frequency;
    float   low_amplitude;
} hoja_haptic_frame_s;

typedef struct
{
    uint8_t sample_count;
    bool unread;
    hoja_haptic_frame_linear_s linear; // Last known state 
    hoja_haptic_frame_s samples[3];
} hoja_rumble_msg_s;

typedef enum
{
    RUMBLE_TYPE_ERM = 0,
    RUMBLE_TYPE_LRA = 1,
    RUMBLE_TYPE_MAX,
} rumble_type_t;

typedef struct
{
    float a_hi;
    float a_lo;
    float f_hi;
    float f_lo;
} amfm_s;

typedef struct 
{
    float frequency_high;
    float amplitude_high;
    float frequency_low;
    float amplitude_low;
} rumble_data_s;

typedef enum
{
  RUMBLE_OFF,
  RUMBLE_BRAKE,
  RUMBLE_ON,
} rumble_t;

uint16_t haptics_mix_pcm(uint16_t pcm_a, uint16_t pcm_b);

bool haptics_set_all(float f_hi, float a_hi, float f_lo, float a_lo);

int8_t haptics_get(bool left, amfm_s* left_out, bool right, amfm_s* right_out);
bool haptics_set(const amfm_s* l_in, int8_t l_count, const amfm_s* r_in, int8_t r_count);

#endif