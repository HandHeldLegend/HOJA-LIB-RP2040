#include "devices/animations/anm_none.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "utilities/settings.h"
#include "board_config.h"

int _rgb_group_leds[HOJA_RGB_GROUPS_NUM][RGB_MAX_LEDS_PER_GROUP] = HOJA_RGB_GROUPINGS;
rgb_s _rgb_groups[HOJA_RGB_GROUPS_NUM];
bool _none_init = false;

void _unpack_groups_to_leds(rgb_s *output)
{
    for(int i = 0; i < HOJA_RGB_GROUPS_NUM; i++)
    {
        for(int j = 0; j < RGB_MAX_LEDS_PER_GROUP; j++)
        {
            int index_out = _rgb_group_leds[i][j];
            if(index_out<0)
            {
                continue;
            }
            else
            {
                output[index_out].color = _rgb_groups[i].color;
            }
        }
    }
}

// Get current rgb state
bool anm_none_get_state(rgb_s *output)
{
    // Reset our static state from stored rgb group data
    for(int i = 0; i < HOJA_RGB_GROUPS_NUM; i++)
    {
        rgb_s tmp_color = {
            .r = (rgb_config->rgb_colors[i] & 0xFF0000) >> 16, 
            .g = (rgb_config->rgb_colors[i] & 0xFF00) >> 8, 
            .b = (rgb_config->rgb_colors[i] & 0xFF)
        };
        // Load our colors from the settings
        _rgb_groups[i].color = tmp_color.color;
    }

    _unpack_groups_to_leds(output);
}

const uint32_t delayed = 24;
uint32_t _delay = delayed;
bool anm_none_handler(rgb_s* output)
{

    _unpack_groups_to_leds(output);
    return true;
}