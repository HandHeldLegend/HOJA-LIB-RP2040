#include "devices/animations/anm_rainbow.h"
#include "devices/animations/anm_utility.h"
#include "devices/rgb.h"
#include "board_config.h"
#include <string.h>

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define NUM_OF_COLORS_RAINBOW 8
rgb_s _rainbow_cols[NUM_OF_COLORS_RAINBOW] = {0};
uint8_t _rainbow_idx_this = 0;
uint8_t _rainbow_idx_next = 1;
rgb_s _this_color = COLOR_RED;

uint32_t _color_fade_idx = 0;
uint32_t _blend_step = 0;

void _generate_rainbow()
{
    hsv_s hsv = {
        .hue = 0,
        .saturation = 255,
        .value = 255
    };
    float hue_step = 255.0f / (float) NUM_OF_COLORS_RAINBOW;
    for (int i = 0; i < NUM_OF_COLORS_RAINBOW; i++)
    {
        hsv.hue = (uint16_t) (hue_step * i);
        anm_utility_hsv_to_rgb(&hsv, &_rainbow_cols[i]);
    }
}

bool anm_rainbow_get_state(rgb_s *output)
{
    _generate_rainbow();

    // Reset our static state from stored rgb group data
    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        output[i].color = _this_color.color;
    }

    _blend_step = anm_utility_get_time_fixedpoint();
    return true;
}

bool anm_rainbow_handler(rgb_s* output)
{
    static uint32_t current_blend = 0;

    _this_color.color = anm_utility_blend(&_rainbow_cols[_rainbow_idx_this], &_rainbow_cols[_rainbow_idx_next], current_blend);

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        output[i].color = _this_color.color;
    }

    current_blend += _blend_step;

    if(current_blend >= RGB_FADE_FIXED_MULT)
    {
        _this_color.color = _rainbow_cols[_rainbow_idx_this].color;
        _rainbow_idx_this = _rainbow_idx_next;
        _rainbow_idx_next = (_rainbow_idx_next + 1) % NUM_OF_COLORS_RAINBOW;
        current_blend = 0;
    }
    return true;
}

#endif