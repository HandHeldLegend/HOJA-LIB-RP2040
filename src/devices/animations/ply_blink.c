#include "devices/animations/ply_blink.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include <stdint.h>

#include "hoja.h"
#include "hoja_shared_types.h"
#include "utilities/settings.h"
#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

typedef enum 
{
    BLINK_DIR_BLACK,
    BLINK_DIR_BRIGHT,
} blink_dir_t;

rgb_s    _blink_color = {.color = 0x00};
rgb_s    _blink_black = {.color = 0x00};

#define PLAYER_BLINK_TIME_FIXED  RGB_FLOAT_TO_FIXED(1.0f /  ANM_UTILITY_GET_FRAMES_FROM_MS(1300) )
#define PLAYER_ONESHOT_TIME_FIXED  RGB_FLOAT_TO_FIXED(1.0f /  ANM_UTILITY_GET_FRAMES_FROM_MS(350) )

bool ply_blink_handler_ss(rgb_s *output, uint8_t size, rgb_s set_color)
{
    static rgb_s       ss_color = {.color = 0};
    static blink_dir_t blink_dir = BLINK_DIR_BLACK;
    static uint32_t    blink_blend = 0;

    // If our current stored color is blank and fading to black (new notification)
    if(!ss_color.color && blink_dir == BLINK_DIR_BLACK)
    {
        for(int i = 0; i < size; i++)
        {
            rgb_s this_color = output[i]; // Get the current color
            output[i].color = anm_utility_blend(&this_color, &_blink_black, blink_blend);
        }

        blink_blend += PLAYER_ONESHOT_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            blink_blend = 0;
            ss_color.color = set_color.color;
            blink_dir = BLINK_DIR_BRIGHT;
        }
    }
    else if (ss_color.color && blink_dir == BLINK_DIR_BRIGHT)
    {   
        for(int i = 0; i < size; i++)
        {
            output[i].color = anm_utility_blend(&_blink_black, &ss_color, blink_blend);
        }

        blink_blend += PLAYER_ONESHOT_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            blink_blend = 0;
            blink_dir = BLINK_DIR_BLACK;
        }
    }
    else if(ss_color.color && blink_dir == BLINK_DIR_BLACK)
    {
        for(int i = 0; i < size; i++)
        {
            output[i].color = anm_utility_blend(&ss_color, &_blink_black, blink_blend);
        }

        blink_blend += PLAYER_ONESHOT_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            ss_color.color = 0;
            blink_blend = 0;
            blink_dir = BLINK_DIR_BRIGHT;
        }
    }
    else
    {
        for(int i = 0; i < size; i++)
        {
            rgb_s this_color = output[i]; // Get the current color
            output[i].color = anm_utility_blend(&_blink_black, &this_color, blink_blend);
        }

        blink_blend += PLAYER_ONESHOT_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            blink_blend = 0;
            blink_dir = BLINK_DIR_BLACK;
            return true;
        }
    }

    return false;
}

bool ply_blink_handler(rgb_s *output, uint8_t size, rgb_s set_color)
{
    static rgb_s       ss_color = {.color = 0};
    static blink_dir_t blink_dir = BLINK_DIR_BLACK;
    static uint32_t    blink_blend = 0;

    if((ss_color.color != set_color.color) && 
    (!blink_blend) && (blink_dir == BLINK_DIR_BRIGHT))
    {
        ss_color.color = set_color.color;
    }

    // If our current stored color is blank and fading to black (new notification)
    if(!ss_color.color && blink_dir == BLINK_DIR_BLACK)
    {
        for(int i = 0; i < size; i++)
        {
            rgb_s this_color = output[i]; // Get the current color
            output[i].color = anm_utility_blend(&this_color, &_blink_black, blink_blend);
        }

        blink_blend += PLAYER_BLINK_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            blink_blend = 0;
            blink_dir = BLINK_DIR_BRIGHT;
        }
    }
    else if (ss_color.color && blink_dir == BLINK_DIR_BRIGHT)
    {   
        for(int i = 0; i < size; i++)
        {
            output[i].color = anm_utility_blend(&_blink_black, &ss_color, blink_blend);
        }

        blink_blend += PLAYER_BLINK_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            blink_blend = 0;
            blink_dir = BLINK_DIR_BLACK;
            return true;
        }
    }
    else if(ss_color.color && blink_dir == BLINK_DIR_BLACK)
    {
        for(int i = 0; i < size; i++)
        {
            output[i].color = anm_utility_blend(&ss_color, &_blink_black, blink_blend);
        }

        blink_blend += PLAYER_BLINK_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            blink_blend = 0;
            blink_dir = BLINK_DIR_BRIGHT;
        }
    }
    else
    {
        for(int i = 0; i < size; i++)
        {
            rgb_s this_color = output[i]; // Get the current color
            output[i].color = anm_utility_blend(&_blink_black, &this_color, blink_blend);
        }

        blink_blend += PLAYER_BLINK_TIME_FIXED;
    
        if(blink_blend >= RGB_FADE_FIXED_MULT)
        {
            blink_blend = 0;
            blink_dir = BLINK_DIR_BLACK;
            return true;
        }
    }

    return false;

}

#endif