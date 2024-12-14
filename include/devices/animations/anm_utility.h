#ifndef ANM_UTILITY_H
#define ANM_UTILITY_H
#include <math.h>
#include "devices_shared_types.h"

uint32_t anm_utility_blend(rgb_s *original, rgb_s *new, float blend);

void anm_utility_process(rgb_s *leds, uint16_t brightness);

#endif