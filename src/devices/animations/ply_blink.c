#include "devices/animations/ply_blink.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include <stdint.h>

#include "hoja.h"
#include "hoja_shared_types.h"
#include "utilities/settings.h"
#include "board_config.h"

uint32_t _blink_blend = 0;
bool     _blink_dir = true;
rgb_s    _blink_black = {.color = 0x00};
rgb_s    _blink_color = {.color = 0x00};
#define PLAYER_BLINK_TIME_FIXED  RGB_FLOAT_TO_FIXED(1.0f /  ANM_UTILITY_GET_FRAMES_FROM_MS(500) )

bool ply_blink_handler(rgb_s *output)
{
    #if defined(HOJA_RGB_PLAYER_GROUP_IDX)

    _blink_color = hoja_gamepad_mode_color_get();

    // Clear all player LEDs
    for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        uint8_t this_idx = rgb_led_groups[HOJA_RGB_PLAYER_GROUP_IDX][i];
        rgb_s this_color = output[this_idx]; // Get the current color
        if(_blink_dir)
        {
            output[this_idx].color = anm_utility_blend(&_blink_color, &_blink_black, _blink_blend);
        }
        else
        {
            output[this_idx].color = anm_utility_blend(&_blink_black, &_blink_color, _blink_blend);
        }
    }

    _blink_blend += PLAYER_BLINK_TIME_FIXED;
    if(_blink_blend >= RGB_FADE_FIXED_MULT)
    {
        _blink_blend = 0;
        _blink_dir = !_blink_dir;

        if(_blink_dir)
        {
            return true;
        }
    }
    return false;

    #endif

    return false;
}