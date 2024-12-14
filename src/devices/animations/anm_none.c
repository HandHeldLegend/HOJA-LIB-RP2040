#include "devices/animations/anm_none.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "utilities/settings.h"
#include "hoja_system.h"

uint8_t _rgb_group_leds[HOJA_RGB_GROUPS_NUM][RGB_MAX_LEDS_PER_GROUP] = HOJA_RGB_GROUPINGS;
uint32_t _rgb_groups[HOJA_RGB_GROUPS_NUM];
bool _none_init = false;

void _unpack_groups_to_leds(uint32_t *output)
{
    for(int i = 0; i < HOJA_RGB_GROUPS_NUM; i++)
    {
        for(int j = 0; j < RGB_MAX_LEDS_PER_GROUP; j++)
        {
            uint8_t index_out = _rgb_group_leds[i][j];
            if(j && !index_out)
            {
                continue;
            }
            else
            {
                output[index_out] = _rgb_groups[i];
            }
        }
    }
}

// Get current rgb state
void anm_none_get_state(uint32_t *output)
{
    // Reset our static state from stored rgb group data
    for(int i = 0; i < HOJA_RGB_GROUPS_NUM; i++)
    {
        // Load our colors from the settings
        _rgb_groups[i] = rgb_config->rgb_colors[i];
    }

    _unpack_groups_to_leds(output);
}

bool anm_none_handler(uint32_t* output)
{

}