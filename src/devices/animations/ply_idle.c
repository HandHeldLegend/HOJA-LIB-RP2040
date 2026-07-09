#include "devices/animations/ply_idle.h"
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
uint32_t _player_change_blend = RGB_FADE_FIXED_MULT;

// Player-number indicator groups are always RGB_PLAYER_GROUP_SIZE (4) LEDs.
bool _off_flags[RGB_PLAYER_GROUP_SIZE] = {0};

const rgb_s _player_num_colors[] = {
    COLOR_RED,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_ORANGE,
    COLOR_CYAN,
    COLOR_PINK,
    COLOR_PURPLE,
};

bool ply_idle_handler(rgb_s *output, int player_num)
{
    static int player_internal = -1;

    if(player_internal != player_num)
    {
        player_internal = player_num;

        // 4 LEDs in the player group, so each led represents a player number bit.
        uint8_t setup = 0b0000;
        switch(player_internal)
        {
            case -1:
            case 0:
                setup = 0;
            break;
            case 1: 
                setup = 0b1;
                break;
            case 2:
                setup = 0b11;
                break;
            case 3:
                setup = 0b111;
                break;
            case 4:
                setup = 0b1111;
                break;
            case 5:
                setup = 0b1001;
                break;
            case 6:
                setup = 0b1010;
                break;
            case 7:
                setup = 0b1011;
                break;
            case 8:
                setup = 0b0110;
                break;
        }

        for (int i = 0; i < RGB_PLAYER_GROUP_SIZE; i++)
        {
            if(!(setup & (1 << i))) 
                _off_flags[i] = true;
            else 
                _off_flags[i] = false;
        }
    }

    for(uint8_t i = 0; i < RGB_PLAYER_GROUP_SIZE; i++)
    {
        if(_off_flags[i])
            output[i].color = 0;
    }

    // Otherwise we do nothing (passthrough)

    return true;
}

#endif
