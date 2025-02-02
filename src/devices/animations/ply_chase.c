#include "devices/animations/ply_chase.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include <stdint.h>
#include <stdbool.h>
#include "hoja.h"
#include "utilities/settings.h"
#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

uint8_t _current_chase_led = 0;
uint8_t _chase_ticks = 0;
bool    _chase_dir = true;
#define LINGER_PLAYER_CHASE_TICKS ANM_UTILITY_GET_FRAMES_FROM_MS(120) // 250ms

bool ply_chase_handler(rgb_s *output, rgb_s set_color)
{
    #if defined(HOJA_RGB_PLAYER_GROUP_IDX)

    bool returnValue = false;

    // Clear all player LEDs
    for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        if(i != _current_chase_led)
            output[i].color = 0x00;
        else 
            output[i] = set_color;
    }

    _chase_ticks++;

    if(_chase_ticks >= LINGER_PLAYER_CHASE_TICKS)
    {
        if(_chase_dir)
        {
            if(_current_chase_led >= (HOJA_RGB_PLAYER_GROUP_SIZE-1))
            {
                _current_chase_led = HOJA_RGB_PLAYER_GROUP_SIZE - 2;
                _chase_dir = false;
                returnValue = true;
            }
            else _current_chase_led++;
        }
        else
        {
            if(!_current_chase_led)
            {
                _current_chase_led = 1;
                _chase_dir = true;
            }
            else _current_chase_led--;
        }
        
        _chase_ticks = 0;
    }

    return returnValue;

    #endif

}

#endif