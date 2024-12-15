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

#define ALL_LEDS_SIZE (sizeof(uint32_t) * RGB_DRIVER_LED_COUNT)

#define FADE_LENGTH_MS 500
#define FADE_LENGTH_FRAMES ((FADE_LENGTH_MS*1000) / RGB_TASK_INTERVAL)
#define FADE_STEP_FLOAT (float) (1.0f / FADE_LENGTH_FRAMES)

// This function handles animation transition, and returns the proper rgb array pointer
typedef bool (*rgb_anim_fn)(rgb_s *);
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

    _fade_progress += FADE_STEP_FLOAT;
    if(_fade_progress>1.0f)
    {
        _fade_progress = 0;
        memcpy(_current_ani_leds, _fade_end, ALL_LEDS_SIZE);
        return true;
    }

    return false;
}

void anm_handler_setup_mode(uint8_t rgb_mode, uint16_t brightness)
{
    _anim_brightness = brightness;

    if(rgb_mode != _current_mode)
    {
        _current_mode = rgb_mode;
        switch(rgb_mode)
        {
            // RGB_ANIM_NONE
            default:
            case 0:
                _ani_main_fn = anm_none_handler;
                _ani_fn_get_state = anm_none_get_state;
                _ani_queue_fade_start();
                return;
            break;
        }
    }
}

// Set this flag to clear the override
bool _force_clear_override = false;

// Call this once per frame
void anm_handler_tick()
{
    // Only compile this function if we have our driver update function
    #if RGB_DEVICE_ENABLED==1
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

    // Process brightness/gamma
    anm_utility_process(_current_ani_leds, _adjusted_ani_leds, _anim_brightness);

    RGB_DRIVER_UPDATE(_adjusted_ani_leds);
    #endif
}

void ani_setup_override(rgb_override_t override, uint32_t *parameters)
{
    
}
