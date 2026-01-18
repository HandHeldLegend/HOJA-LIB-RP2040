#include "devices/animations/anm_none.h"
#include "devices/animations/anm_utility.h"
#include "devices/rgb.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "utilities/settings.h"
#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

//int _rgb_group_leds[HOJA_RGB_GROUPS_NUM][RGB_MAX_LEDS_PER_GROUP] = HOJA_RGB_GROUPINGS;
rgb_s _rgb_groups[HOJA_RGB_GROUPS_NUM];
bool _none_init = false;

// Get current rgb state
bool anm_none_get_state(rgb_s *output)
{
    // Reset our static state from stored rgb group data
    for(int i = 0; i < HOJA_RGB_GROUPS_NUM; i++)
    {
        rgb_s tmp_color = rgb_colors_safe[i];
        // Load our colors from the settings
        _rgb_groups[i].color = tmp_color.color;
    }

    anm_utility_unpack_groups_to_leds(output, _rgb_groups);
}

const uint32_t delayed = 24;
uint32_t _delay = delayed;
bool anm_none_handler(rgb_s* output)
{
    anm_utility_unpack_groups_to_leds(output, _rgb_groups);
    return true;
}

#endif