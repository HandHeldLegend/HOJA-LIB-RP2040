#include "devices/animations/ply_idle.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include <stdint.h>
#include <stdbool.h>
#include "utilities/settings.h"
#include "hoja.h"
#include "hoja_shared_types.h"
#include "board_config.h"

#define PLAYER_NUM_CHANGE_STEP_FIXED RGB_FLOAT_TO_FIXED(1.0f /  ANM_UTILITY_GET_FRAMES_FROM_MS(200) )

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
    #if defined(HOJA_RGB_PLAYER_GROUP_SIZE)
    static int player_internal = -1;

    if(player_internal != player_num)
    {
        player_internal = player_num;
        //player_change_blend = RGB_FADE_FIXED_MULT;
    }


    #if (HOJA_RGB_PLAYER_GROUP_SIZE == 4)
    // If we have 4 LEDs in the player group,
    // we can set each led to represent a player number
    uint8_t setup = 0b0000;
    switch(player_internal)
    {
        case -1:
        case 0:
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

    for (int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        uint8_t group_idx = rgb_led_groups[HOJA_RGB_PLAYER_GROUP_IDX][i];
        if(!(setup & (1 << i))) 
            output[group_idx].color = 0;
    }
    #else
    // If we have a different number of LEDs in the player group
    //rgb_s new_color_plyr = _player_num_colors[player_internal];
    //for (int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    //{
    //    uint8_t group_idx = rgb_led_groups[HOJA_RGB_PLAYER_GROUP_IDX][i];
    //    output[group_idx].color = new_color_plyr.color;
    //}
    #endif


    return true;

    #endif
}