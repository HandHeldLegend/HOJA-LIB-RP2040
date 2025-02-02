#include "devices/animations/ply_blink.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include <stdint.h>

#include "hoja.h"
#include "hoja_shared_types.h"
#include "utilities/settings.h"
#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

uint32_t _blink_blend = 0;
bool     _blink_dir = false;
rgb_s    _blink_color = {.color = 0x00};
rgb_s    _blink_black = {.color = 0x00};
#define PLAYER_BLINK_TIME_FIXED  RGB_FLOAT_TO_FIXED(1.0f /  ANM_UTILITY_GET_FRAMES_FROM_MS(500) )

bool ply_blink_handler(rgb_s *output, rgb_s set_color)
{
    #if defined(HOJA_RGB_PLAYER_GROUP_IDX)
    for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        rgb_s this_color = output[i]; // Get the current color
        if(_blink_dir)
        {
            output[i].color = anm_utility_blend(&set_color, &_blink_black, _blink_blend);
        }
        else
        {
            output[i].color = anm_utility_blend(&_blink_black, &set_color, _blink_blend);
        }
    }

    _blink_blend += PLAYER_BLINK_TIME_FIXED;
    if(_blink_blend >= RGB_FADE_FIXED_MULT)
    {
        if(_blink_dir)
        {
            _blink_blend = 0;
            _blink_dir = false;
            return true;
        }

        _blink_blend = 0;
        _blink_dir = true;
    }
    return false;

    #endif

    return false;
}

#endif