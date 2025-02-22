#include "devices/animations/anm_fairy.h"
#include "devices/animations/anm_utility.h"
#include "devices/rgb.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "utilities/settings.h"
#include "board_config.h"

#include "hal/sys_hal.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define FAIRY_NUM_OPTIONS 6

uint8_t _current_cycle_idx[RGB_DRIVER_LED_COUNT] = {0};
uint8_t _next_offset_idx[RGB_DRIVER_LED_COUNT] = {0};
uint32_t _fairy_fade_idx = 0;
uint32_t _fairy_blend_step = 0;

// Get current rgb state
bool anm_fairy_get_state(rgb_s *output)
{
    static bool fairy_boot = false;
    _fairy_blend_step = anm_utility_get_time_fixedpoint();

    // Reset our static state from stored rgb group data
    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {   
        if (!fairy_boot)
        {
            _current_cycle_idx[i] = sys_hal_random() % FAIRY_NUM_OPTIONS;
            _next_offset_idx[i]   = sys_hal_random() % FAIRY_NUM_OPTIONS;
        }
        output[i].color = rgb_colors_safe[_current_cycle_idx[i]].color;
    }
    fairy_boot = true;
    return true;
}

bool anm_fairy_handler(rgb_s* output)
{
    static uint32_t current_blend = 0;

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        output[i].color = anm_utility_blend(&rgb_colors_safe[_current_cycle_idx[i]], &rgb_colors_safe[_next_offset_idx[i]], current_blend);
    }

    current_blend += _fairy_blend_step;

    if(current_blend >= RGB_FADE_FIXED_MULT)
    {
        for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
        {
            _current_cycle_idx[i]   = _next_offset_idx[i];
            _next_offset_idx[i]     = sys_hal_random() % FAIRY_NUM_OPTIONS;
        }
        current_blend = 0;
    }

    return true;
}

#endif