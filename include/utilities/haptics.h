#ifndef HAPTICS_H
#define HAPTICS_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_types.h"

uint16_t haptics_mix_pcm(uint16_t pcm_a, uint16_t pcm_b);

bool haptics_set_all(float f_hi, float a_hi, float f_lo, float a_lo);

int8_t haptics_get(bool left, amfm_s* left_out, bool right, amfm_s* right_out);
bool haptics_set(const amfm_s* l_in, int8_t l_count, const amfm_s* r_in, int8_t r_count);

#endif