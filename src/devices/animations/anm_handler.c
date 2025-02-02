#include "devices/animations/anm_handler.h"
#include "devices/animations/anm_utility.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hoja.h"

#include "board_config.h"

#include "devices/rgb.h"
#include "hal/rgb_hal.h"
#include "utilities/settings.h"

// Primary animation modes
#include "devices/animations/anm_none.h"
#include "devices/animations/anm_breathe.h"
#include "devices/animations/anm_fairy.h"
#include "devices/animations/anm_rainbow.h"
#include "devices/animations/anm_react.h"
#include "devices/animations/anm_shutdown.h"

// Player LED animation modes
#include "devices/animations/ply_chase.h"
#include "devices/animations/ply_blink.h"
#include "devices/animations/ply_idle.h"
#include "devices/animations/ply_shutdown.h"

// Overrides
#include "devices/animations/or_flash.h"
#include "devices/animations/or_indicate.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define ALL_LEDS_SIZE (sizeof(uint32_t) * RGB_DRIVER_LED_COUNT)

#define FADE_LENGTH_MS 500
#define FADE_LENGTH_FRAMES  ((FADE_LENGTH_MS*1000) / RGB_TASK_INTERVAL)
#define FADE_STEP_FIXED     RGB_FLOAT_TO_FIXED(1.0f / FADE_LENGTH_FRAMES)

// This function handles animation transition, and returns the proper rgb array pointer
typedef bool (*rgb_anim_fn)(rgb_s *);
typedef void (*rgb_anim_stop_fn)(void);

rgb_anim_fn         _ani_main_fn = NULL; // The main operating function of the current rgb mode
rgb_anim_fn         _ani_fn_get_state = NULL; // Get the current state of the active mode
rgb_anim_stop_fn    _ani_fn_stop = NULL; // Call this to stop/pause the rgb state

rgb_anim_fn         _ani_override_fn = NULL; // Override function
rgb_anim_stop_fn    _ani_override_stop_fn = NULL;

typedef enum 
{
    RGB_ANIM_NONE,      // Show stored colors static
    RGB_ANIM_RAINBOW,   // Fade through the RGB rainbow
    RGB_ANIM_REACT,     // React to user input
    RGB_ANIM_FAIRY,     // Play a random animation between user preset colors
    RGB_ANIM_BREATHE,   // Looping animation using stored colors
} rgb_anim_t;

typedef enum 
{
    RGB_OVERRIDE_FLASH,
    RGB_OVERRIDE_INDICATE,
    RGB_OVERRIDE_SHUTDOWN,
} rgb_override_t;

uint32_t _fade_progress  = 0;

rgb_s    _fade_start[RGB_DRIVER_LED_COUNT]   = {0};
rgb_s    _fade_end[RGB_DRIVER_LED_COUNT]     = {0};

rgb_s    _current_ani_leds[RGB_DRIVER_LED_COUNT] = {0};
rgb_s    _adjusted_ani_leds[RGB_DRIVER_LED_COUNT] = {0};


uint16_t _anim_brightness = 0;
int _current_mode = -1;

bool _fade = false;
void _ani_queue_fade_start()
{
    memcpy(_fade_start, _current_ani_leds, ALL_LEDS_SIZE);
    _ani_fn_get_state(_fade_end);
    _fade_progress = 0;
    _fade = true;
}

bool _ani_queue_fade_handler()
{
    // Write the state of the current mode to our fade start
    // This allows our current main mode to keep operating
    // even during a transition

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        _current_ani_leds[i].color = anm_utility_blend(&(_fade_start[i]), &(_fade_end[i]), _fade_progress);
    }

    _fade_progress += FADE_STEP_FIXED;
    if(_fade_progress>=RGB_FADE_FIXED_MULT)
    {
        _fade_progress = 0;
        memcpy(_current_ani_leds, _fade_end, ALL_LEDS_SIZE);
        return true;
    }

    return false;
}

void anm_handler_shutdown(callback_t cb)
{
    anm_shutdown_set_cb(cb);
    _ani_main_fn = anm_shutdown_handler;
    _ani_fn_get_state = anm_shutdown_get_state;
    _ani_queue_fade_start();
}

void anm_handler_setup_mode(uint8_t rgb_mode, uint16_t brightness, uint32_t animation_time_ms)
{
    anm_utility_set_time_ms(animation_time_ms);

    _anim_brightness = brightness;

    _current_mode = rgb_mode;
    switch(rgb_mode)
    {        
        // RGB_ANIM_NONE
        default:
        case RGB_ANIM_NONE:
            _ani_main_fn = anm_none_handler;
            _ani_fn_get_state = anm_none_get_state;
            _ani_queue_fade_start();
            return;
        break;

        case RGB_ANIM_RAINBOW:
            _ani_main_fn = anm_rainbow_handler;
            _ani_fn_get_state = anm_rainbow_get_state;
            _ani_queue_fade_start();
            return;
        break;

        case RGB_ANIM_REACT:
            _ani_main_fn = anm_react_handler;
            _ani_fn_get_state = anm_react_get_state;
            _ani_queue_fade_start();
            return;
        break;
    }
}

void _player_connection_manager(rgb_s *output) 
{
    #if defined(HOJA_RGB_PLAYER_GROUP_IDX)
    static bool allow_update = true;
    static hoja_status_s status = {0};
    rgb_s player_leds[HOJA_RGB_PLAYER_GROUP_SIZE] = {0};

    // Get the current player LEDs
    for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        uint8_t this_idx = rgb_led_groups[HOJA_RGB_PLAYER_GROUP_IDX][i];
        player_leds[i] = output[this_idx];
    }

    if(status.notification_color.color > 0)
    {
        allow_update = ply_blink_handler(player_leds, status.notification_color);
    }
    else
    {
        switch(status.connection_status)
        {
            case CONN_STATUS_INIT:
                #if defined(HOJA_RGB_PLAYER_GROUP_SIZE)
                // Clear all player LEDs
                for(int i = 0; i < i; i++)
                {
                    player_leds[i].color = 0;
                }
                #endif
            break;

            case CONN_STATUS_DISCONNECTED:
            case CONN_STATUS_CONNECTING:
                #if defined(HOJA_RGB_PLAYER_GROUP_SIZE) && (HOJA_RGB_PLAYER_GROUP_SIZE >= 4)
                allow_update = ply_chase_handler(player_leds, status.gamepad_color);
                #else 
                allow_update = ply_blink_handler(player_leds, status.gamepad_color);
                #endif
            break;

            case CONN_STATUS_SHUTDOWN:
                allow_update = ply_shutdown_handler(player_leds);
            break;

            default:
                allow_update = ply_idle_handler(player_leds, status.connection_status);
            break;
        }        
    }

    if(allow_update)
    {
        status = hoja_get_status();
    }

    // Write the player LED colors to the output
    for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        uint8_t this_idx = rgb_led_groups[HOJA_RGB_PLAYER_GROUP_IDX][i];
        output[this_idx] = player_leds[i];
    }
    #endif
}

// Call this once per frame
void anm_handler_tick()
{
    // Only compile this function if we have our driver update function
    #if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER>0)
    if(_fade)
    {
        if(_ani_queue_fade_handler(_current_ani_leds))
        {
            _fade = false;
        }
    }
    else if(_ani_main_fn != NULL)
    {
        _ani_main_fn(_current_ani_leds);
    }

    // Ensure these show no matter the brightness
    _player_connection_manager(_current_ani_leds);

    // Process brightness/gamma
    anm_utility_process(_current_ani_leds, _adjusted_ani_leds, _anim_brightness);

    RGB_DRIVER_UPDATE(_adjusted_ani_leds);
    #endif
}

void ani_setup_override(rgb_override_t override, uint32_t *parameters)
{
    
}

#endif
