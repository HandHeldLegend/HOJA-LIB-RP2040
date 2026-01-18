#ifndef ANM_UTILITY_H
#define ANM_UTILITY_H
#include <math.h>
#include "devices_shared_types.h"

#define MS_TO_US(ms) (ms * 1000)
#define ANM_UTILITY_GET_FRAMES_FROM_MS(ms) (MS_TO_US(ms) / RGB_TASK_INTERVAL)

void anm_utility_apply_color_safety(rgb_s *color);
uint32_t anm_utility_pack_local_color(rgb_s color);
rgb_s anm_utility_unpack_local_color(uint32_t color);
uint32_t anm_utility_get_time_fixedpoint();
uint32_t anm_utility_ms_to_frames(uint32_t time_ms);
void anm_utility_set_time_ms(uint32_t time_ms);
uint32_t anm_utility_blend(rgb_s *original, rgb_s *new, uint32_t blend);
void anm_utility_process(rgb_s *in, rgb_s *out, uint16_t brightness);
void anm_utility_hsv_to_rgb(hsv_s *hsv, rgb_s *out);
void anm_utility_unpack_groups_to_leds(rgb_s *output, rgb_s rgb_groups[HOJA_RGB_GROUPS_NUM]);

#endif