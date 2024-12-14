#include "devices/animations/anm_handler.h"
#include "devices/animations/anm_utility.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hoja_system.h"
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

// Overrides
#include "devices/animations/or_flash.h"
#include "devices/animations/or_indicate.h"

// This function handles animation transition, and returns the proper rgb array pointer
typedef bool (*rgb_anim_fn)(uint32_t *);
typedef void (*rgb_anim_stop_fn)(void);

rgb_anim_fn _ani_main_fn = NULL; // The main operating function of the current rgb mode
rgb_anim_fn _ani_fn_get_state = NULL; // Get the current state of the active mode
rgb_anim_stop_fn _ani_fn_stop = NULL; // Call this to stop/pause the rgb state

rgb_anim_fn _ani_override_fn = NULL; // Override function
rgb_anim_stop_fn _ani_override_stop_fn = NULL;

typedef enum 
{
    RGB_ANIM_NONE,  // Show stored colors static
    RGB_ANIM_RAINBOW, // Fade through the RGB rainbow
    RGB_ANIM_FAIRY, // Play a random animation between user preset colors
    RGB_ANIM_BREATHE, // Looping animation using stored colors
    RGB_ANIM_REACT, // Not used for now
} rgb_anim_t;

typedef enum 
{
    RGB_OVERRIDE_FLASH,
    RGB_OVERRIDE_INDICATE,
    RGB_OVERRIDE_SHUTDOWN,
} rgb_override_t;

float       _fade_progress  = 0;

uint32_t    _fade_start[RGB_DRIVER_COUNT]   = {0};
uint32_t    _fade_end[RGB_DRIVER_COUNT]     = {0};

uint32_t    _current_ani_leds[RGB_DRIVER_COUNT] = {0};

bool _fade = false;
void _ani_queue_fade_start()
{
    memcpy(_fade_start, _current_ani_leds, ALL_LEDS_SIZE);
    _ani_fn_get_state(_fade_end);
    _fade_progress = 0;
    _fade = true;
}

bool _ani_queue_fade_handler(uint32_t *rgb_out)
{
    // Write the state of the current mode to our fade start
    // This allows our current main mode to keep operating
    // even during a transition

    for(int i = 0; i < RGB_DRIVER_COUNT; i++)
    {
        rgb_out[i] = anm_utility_blend(&(_fade_start[i]), &(_fade_end[i]), _fade_progress);
    }

    _fade_progress += FADE_STEP_FLOAT;
    if(_fade_progress>1.0f)
    {
        _fade_progress = 0;
        memcpy(rgb_out, _fade_end, ALL_LEDS_SIZE);
        return true;
    }

    return false;
}

int _current_mode = -1;

void ani_setup_mode(uint8_t rgb_mode)
{
    if(rgb_mode != _current_mode)
    {
        switch(rgb_mode)
        {
            // RGB_ANIM_NONE
            case 0:
                
            break;
        }
    }
}

// Set this flag to clear the override
bool _force_clear_override = false;
void ani_handler()
{
    if(_fade)
    {
        if(_ani_queue_fade_handler(_current_ani_leds))
        {
            _fade = false;
        }
    }
    else if(_ani_override_fn != NULL)
    {
        // Enter if our override function returned true
        if(_ani_override_fn(_current_ani_leds) || _force_clear_override)
        {
            // Stop the override
            _ani_override_stop_fn();

            // Clear our override
            _ani_override_fn = NULL;
            _ani_override_stop_fn = NULL;
            _force_clear_override = false;
            
            _ani_queue_fade_start();
        }
    }
    else if(_ani_main_fn != NULL)
    {
        _ani_main_fn(_current_ani_leds);
    }
    #if defined(RGB_DRIVER_UPDATE)
    RGB_DRIVER_UPDATE(_current_ani_leds);
    #endif
}

void ani_setup_override(rgb_override_t override, uint32_t *parameters)
{
    switch(override)
    {
        case RGB_OVERRIDE_FLASH:
            // First param is our target color
            memset(_fade_end, parameters[0], ALL_LEDS_SIZE);
            or_flash_load(parameters[0]);
            tmp_anim.loop = true;
            tmp_anim.function = or_flash_handler;
            _ani_add_to_queue(&tmp_anim);
        break;

        case RGB_OVERRIDE_INDICATE:
            // No flash animation for this one
            or_indicate_load(parameters[0]);
            memset(_fade_end, parameters[0], ALL_LEDS_SIZE);
            tmp_anim.function = or_indicate_handler;
        break;
    }

    // Add our original animation for after
}
