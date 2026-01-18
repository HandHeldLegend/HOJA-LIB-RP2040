#include "devices/animations/anm_external.h"
#include "devices/animations/anm_utility.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "devices_shared_types.h"
#include "board_config.h"
#include "input/idle_manager.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define ALL_GROUPS_SIZE sizeof(rgb_s) * HOJA_RGB_GROUPS_NUM

rgb_s _rgb_queue[HOJA_RGB_GROUPS_NUM] = {0};
rgb_s _rgb_display[HOJA_RGB_GROUPS_NUM] = {0};

bool anm_external_queue_rgb_group(uint8_t group_idx, rgb_s rgb_color) {
    if (group_idx >= HOJA_RGB_GROUPS_NUM)
        return false;

    _rgb_queue[group_idx].color = rgb_color.color;
    return true;
}

bool anm_external_queue_rgb_group_range(uint8_t grp_idx_start, uint8_t grp_idx_end, rgb_s rgb_color) {
    for (int i = grp_idx_start; i <= grp_idx_end; i++) {
        if (i >= HOJA_RGB_GROUPS_NUM)
            return false;

        _rgb_queue[i].color = rgb_color.color;
    }
    return true;
}

void anm_external_dequeue() {
    memcpy(_rgb_display, _rgb_queue, ALL_GROUPS_SIZE);
}

bool anm_external_get_state(rgb_s* output) {
    anm_utility_unpack_groups_to_leds(output, _rgb_display);
    return true;
}

bool anm_external_handler(rgb_s* output)
{
    idle_manager_heartbeat();
    anm_utility_unpack_groups_to_leds(output, _rgb_display);
    return true;
}

#endif

