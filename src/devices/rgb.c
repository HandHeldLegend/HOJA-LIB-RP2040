#include "devices/rgb.h"

#include "utilities/interval.h"
#include "utilities/settings.h"

// Include all RGB drivers
#include "hal/rgb_hal.h"

#include "devices/animations/anm_handler.h"

/**
void _rgb_set_player(uint8_t player_number)
{
    if( (_player_size>=4) && (_rgb_group_player[0]>-1) )
    {
        uint8_t setup = 0b0000;
        switch(player_number)
        {
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
        
        if(!(setup & 0b1)) _rgb_next[_rgb_group_player[0]].color = 0x00;
        if(!(setup & 0b10)) _rgb_next[_rgb_group_player[1]].color = 0x00;
        if(!(setup & 0b100)) _rgb_next[_rgb_group_player[2]].color = 0x00;
        if(!(setup & 0b1000)) _rgb_next[_rgb_group_player[3]].color = 0x00;
    }
    else
    {
        rgb_s player_color = {0};

        switch(player_number)
        {
            case 0:
            break;
            case 1: 
                player_color.color = COLOR_RED.color;
                break;
            case 2:
                player_color.color = COLOR_BLUE.color;
                break;
            case 3:
                player_color.color = COLOR_YELLOW.color;
                break;
            case 4:
                player_color.color = COLOR_GREEN.color;
                break;
            case 5:
                player_color.color = COLOR_ORANGE.color;
                break;
            case 6:
                player_color.color = COLOR_CYAN.color;
                break;
            case 7:
                player_color.color = COLOR_PINK.color;
                break;
            case 8:
                player_color.color = COLOR_PURPLE.color;
                break;
        }
        rgb_s blend = {.color=0x00};
        rgb_s new = {.color = _rgb_blend(&player_color, &blend, 0.9f)};

        for(uint i = 0; i < _player_size; i++)
        {
            _rgb_next[_rgb_group_player[i]].color = new.color;
        }
    }
}

void rgb_set_player(uint8_t player_number)
{
    #if (HOJA_CAPABILITY_RGB)
    _rgb_player_number = player_number;
    _rgb_set_player(_rgb_player_number);
    _rgb_mode_setup = false;
    _rgb_set_dirty();
    #endif
}
**/

void rgb_set_player(uint8_t player)
{
    (void) player;
}

// This function will start a shutdown
// that includes an LED fade-out animation
// This ensures the LEDs properly turn off before power off.
// Pass in a callback function which will be called when it's done.
void rgb_shutdown_init(bool restart, callback_t cb)
{

    /*_rgb_shutdown_restart = restart;
    _rgb_anim_cb = NULL;
    _rgb_shutdown_cb = cb;

    rgb_set_override(_rgb_shutdown_start_override_do, 25);*/
}

const rgb_s _rainbow[] = COLORS_RAINBOW;

uint32_t rgb_pack_local_color(rgb_s color)
{
    return  
    (color.r << 16) |
    (color.g << 8) |
    (color.b);
}

void rgb_init(int mode, int brightness)
{
    #if defined(RGB_DRIVER_INIT)
    uint8_t set_mode = 0;
    uint16_t set_brightness = 0;

    // Handle defaulting if we don't have colors
    if(!rgb_config->rgb_config_version)
    {
        uint8_t col = 0;
        const uint8_t color_count = 7;
        for(int i = 0; i < 32; i++)
        {
            rgb_config->rgb_colors[i] = rgb_pack_local_color(_rainbow[col]);
            col = (col+1) % color_count;
        }
        mode = 0;
        brightness = 0;

        set_mode = 0;
        set_brightness = 100;
    }
    
    static bool _rgb_ll_init = false;
    if(!_rgb_ll_init)
    {
        RGB_DRIVER_INIT();
        _rgb_ll_init = true;
    }

    if(mode<0)
    {
        set_mode = rgb_config->rgb_mode;
    }

    if(brightness<0)
    {
        set_brightness = rgb_config->rgb_brightness;
    }

    anm_handler_setup_mode(set_mode, set_brightness);

    #endif
}

// One tick of RGB logic
// only performs actions if necessary
void rgb_task(uint32_t timestamp)
{
    #if (RGB_DEVICE_ENABLED == 1)
    static interval_s interval = {0};

    if (interval_run(timestamp, RGB_TASK_INTERVAL, &interval))
    {
        anm_handler_tick();
    }
    #endif
}

void rgb_config_command(rgb_cmd_t cmd)
{
    switch(cmd)
    {
        default:
        break;

        case RGB_CMD_REFRESH:
            rgb_init(-1, -1);
        break;
    }
}