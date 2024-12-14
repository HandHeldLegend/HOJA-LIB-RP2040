
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hoja_system.h"
#include "devices/rgb.h"
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

#define ALL_LEDS_SIZE sizeof(uint32_t) * RGB_DRIVER_COUNT

rgb_anim_fn _ani_main_fn = NULL;
rgb_anim_fn _ani_fn_get_state = NULL;
rgb_anim_stop_fn _ani_fn_stop = NULL;

rgb_anim_fn _ani_override_fn = NULL;

uint32_t _rgb_blend(rgb_s *original, rgb_s *new, float blend)
{
    float or = (float)original->r;
    float og = (float)original->g;
    float ob = (float)original->b;
    float nr = (float)new->r;
    float ng = (float)new->g;
    float nb = (float)new->b;
    float outr = or +((nr - or) * blend);
    float outg = og + ((ng - og) * blend);
    float outb = ob + ((nb - ob) * blend);
    uint8_t outr_int = (uint8_t)outr;
    uint8_t outg_int = (uint8_t)outg;
    uint8_t outb_int = (uint8_t)outb;
    rgb_s col = {
        .r = outr_int,
        .g = outg_int,
        .b = outb_int};
    return col.color;
}

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

// Convert milliseconds to frames according to our refresh interval
uint32_t ms_to_frames(uint32_t refresh_interval, uint32_t time_ms)
{

}

float       _fade_step      = 0.1f;
float       _fade_progress  = 0;

uint32_t    _fade_start[RGB_DRIVER_COUNT]   = {0};
uint32_t    _fade_end[RGB_DRIVER_COUNT]     = {0};

uint32_t    _current_ani_leds[RGB_DRIVER_COUNT] = {0};

void _ani_queue_fade_start(uint32_t *start_state)
{
    // Calculate _fade_step

    memcpy(_fade_start, start_state, ALL_LEDS_SIZE);
    _fade_progress = 0;
}

bool _ani_queue_fade_handler(uint32_t *rgb_out)
{
    for(int i = 0; i < RGB_DRIVER_COUNT; i++)
    {
        rgb_out[i] = _rgb_blend(&(_fade_start[i]), &(_fade_end[i]), _fade_progress);
    }

    _fade_progress += _fade_step;
    if(_fade_progress>1.0f)
    {
        memcpy(rgb_out, _fade_end, ALL_LEDS_SIZE);
        return true;
    }

    return false;
}

bool _fade = false;
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
            // Clear our override
            _ani_override_fn = NULL;
            _force_clear_override = false;
            
            memcpy(_fade_start, _current_ani_leds, ALL_LEDS_SIZE);

            if(_ani_fn_get_state != NULL)
                _ani_fn_get_state(_fade_end);
        }
    }
    else if(_ani_main_fn != NULL)
    {

    }
    else 
    {

        // Boot time load animation
        uint8_t mode = rgb_config->rgb_mode;
        switch(mode)
        {
            case 0:
            break;
        }
    }
}

uint32_t ani_get_fade_frames()
{

}



rgb_anim_t  _current_type = RGB_ANIM_NONE;
bool        _next_animation_go = false;

void ani_setup_override(rgb_override_t override, uint32_t *parameters)
{
    // The concept here is fairly simple, we want to fade into our
    // overriede, handle the override, then fade back into our regularly
    // scheduled programming

    rgb_anim_s tmp_anim = {0};

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

void ani_queue_play(rgb_anim_t anim)
{
    rgb_anim_s tmp_anim = {0};

    switch(anim)
    {
        case RGB_ANIM_NONE:
            // Set animation start state
            ani_none_get_state(tmp_anim.start_state);
            tmp_anim.function = anm_none_handler;
            tmp_anim.loop = true;
            _ani_add_to_queue(&tmp_anim);
        break;

        case RGB_ANIM_SHUTDOWN:
            // Nullify start state. We just go from
            // whatever state we last had and fade
            memset(tmp_anim.start_state, NULL, ALL_LEDS_SIZE);
            tmp_anim.function = anm_shutdown_handler;
            tmp_anim.loop = true;
            _ani_add_to_queue(&tmp_anim);
        break;

        case RGB_ANIM_RAINBOW:
        break;

        case RGB_ANIM_BREATHE:
        break;
    }
}