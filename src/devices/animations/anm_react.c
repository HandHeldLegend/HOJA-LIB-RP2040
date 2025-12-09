#include "devices/animations/anm_react.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include "input/analog.h"

#include "input/mapper.h"
#include "utilities/settings.h"
#include "utilities/static_config.h"

#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

uint32_t _color_fades[MAPPER_INPUT_COUNT] = {0};
uint16_t _max_values[MAPPER_INPUT_COUNT] = {0};
uint32_t _color_fade_step = 0;
rgb_s color_black = {.color = 0x00};

int8_t _rgb_react_group_leds[HOJA_RGB_GROUPS_NUM][RGB_MAX_LEDS_PER_GROUP] = HOJA_RGB_GROUPINGS;

static inline void _handle_analog_input(uint16_t input_value, const uint8_t group_idx, rgb_s *output) {
    // Cap input value to 12-bit range (0-4095)
    if(input_value > 4094) {
        input_value = 4094;
    }
    else if(input_value < 1000) {
        input_value = 0;
    }

    // Update max value if current input is higher
    if(input_value >= _max_values[group_idx]) {
        _max_values[group_idx] = input_value;
        _color_fades[group_idx] = 0; // Reset fade when new max reached
    } 
    else if(input_value < _max_values[group_idx]) {
        // Increment fade when current value is less than last max
        _color_fades[group_idx] += _color_fade_step;
        
        // Cap fade at maximum
        if(_color_fades[group_idx] >= RGB_FADE_FIXED_MULT) {
            _color_fades[group_idx] = RGB_FADE_FIXED_MULT;
            _max_values[group_idx] = input_value; // Reset max to current once fully faded
        }
    }

    // Calculate base brightness from current input value (0-4095 mapped to 0-RGB_FADE_FIXED_MULT)
    uint32_t current_brightness = ((uint32_t)input_value * RGB_FADE_FIXED_MULT) / 4095;
    
    // Calculate fade brightness from max value
    uint32_t fade_brightness = ((uint32_t)_max_values[group_idx] * RGB_FADE_FIXED_MULT) / 4095;
    
    // Apply fade to the fade brightness
    uint32_t faded_intensity = (fade_brightness * (RGB_FADE_FIXED_MULT - _color_fades[group_idx])) / RGB_FADE_FIXED_MULT;
    
    // Take the maximum of current brightness and faded peak brightness
    // This ensures we show current input immediately while fading out the peak
    uint16_t final_intensity = (current_brightness > faded_intensity) ? current_brightness : faded_intensity;

    // Obtain our group color
    rgb_s group_color = rgb_colors_safe[group_idx];
    
    // Blend between group color and black based on final intensity
    rgb_s blend = {.color = anm_utility_blend(&group_color, &color_black, RGB_FADE_FIXED_MULT - final_intensity)};

    // Write color to output according to group
    for(int i = 0; i < RGB_MAX_LEDS_PER_GROUP; i++) {
        int8_t idx_out = _rgb_react_group_leds[group_idx][i];
        if(idx_out < 0) {
            continue;
        }
        output[idx_out].color = blend.color;
    }
}

bool anm_react_get_state(rgb_s *output)
{
    _color_fade_step = anm_utility_get_time_fixedpoint();
    // Reset our static state from stored rgb group data
    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        _color_fades[i] = RGB_FADE_FIXED_MULT;
    }

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        output[i].color = 0x00;
    }

    return true;
}

uint16_t _parse_distance(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2)
{
    uint16_t max_x = (x1 > x2) ? x1 : x2;
    uint16_t max_y = (y1 > y2) ? y1 : y2;
    return (max_x > max_y) ? max_x : max_y;
}

bool anm_react_handler(rgb_s* output)
{
    mapper_input_s input = mapper_get_translated_input();
    
    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        uint8_t group = input_static.input_info[i].rgb_group;
        if(group > 0)
        {
            group-=1;

            switch(i)
            {
                case INPUT_CODE_LX_RIGHT:
                uint16_t d1 = _parse_distance(input.inputs[INPUT_CODE_LX_LEFT], input.inputs[INPUT_CODE_LX_RIGHT], input.inputs[INPUT_CODE_LY_UP], input.inputs[INPUT_CODE_LY_DOWN]);
                _handle_analog_input(d1, group, output);
                break;

                case INPUT_CODE_RX_RIGHT:
                uint16_t d2 = _parse_distance(input.inputs[INPUT_CODE_RX_LEFT], input.inputs[INPUT_CODE_RX_RIGHT], input.inputs[INPUT_CODE_RY_UP], input.inputs[INPUT_CODE_RY_DOWN]);
                _handle_analog_input(d2, group, output);
                break;

                default:
                _handle_analog_input(input.inputs[i] & 0x1FFF, group, output);
                break;
            }
        }
    }
    return true;
}

#endif