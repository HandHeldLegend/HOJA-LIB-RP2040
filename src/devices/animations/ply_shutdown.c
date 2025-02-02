#include "devices/animations/ply_shutdown.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utilities/settings.h"
#include "hoja.h"
#include "hoja_shared_types.h"
#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define PLAYER_NUM_CHANGE_STEP_FIXED RGB_FLOAT_TO_FIXED(1.0f /  ANM_UTILITY_GET_FRAMES_FROM_MS(500) )
int32_t _player_change_blend_shutdown = RGB_FADE_FIXED_MULT;
rgb_s _black = {.color = 0x00};

bool _started = false;

bool ply_shutdown_handler(rgb_s *output)
{
    #if defined(HOJA_RGB_PLAYER_GROUP_IDX)
    for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        output[i].color = anm_utility_blend(&_black, &output[i], _player_change_blend_shutdown);
    }

    _player_change_blend_shutdown -= PLAYER_NUM_CHANGE_STEP_FIXED;
    if(_player_change_blend_shutdown <= 0)
    {
        _player_change_blend_shutdown = 0;
    }
    #endif
    return true;
}

#endif
