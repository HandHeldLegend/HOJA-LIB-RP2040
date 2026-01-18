#ifndef ANM_EXTERNAL_H
#define ANM_EXTERNAL_H
#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"

bool anm_external_queue_rgb_group(uint8_t group_idx, rgb_s rgb_color);
bool anm_external_queue_rgb_group_range(uint8_t grp_idx_start, uint8_t grp_idx_end, rgb_s rgb_color);
void anm_external_dequeue();
bool anm_external_get_state(rgb_s* output);
bool anm_external_handler(rgb_s* output);

#endif
