#include "devices/animations/anm_react.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include "input/analog.h"

#include "utilities/settings.h"

#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define RGB_REMAP_IDX_NULL    -1
#define RGB_REMAP_IDX_DPAD    4
#define RGB_REMAP_IDX_STICK_L     5 
#define RGB_REMAP_IDX_STICK_R     6 
#define RGB_REMAP_IDX_SOUTH       0
#define RGB_REMAP_IDX_EAST        1 
#define RGB_REMAP_IDX_WEST        2 
#define RGB_REMAP_IDX_NORTH       3
#define RGB_REMAP_IDX_L       7
#define RGB_REMAP_IDX_R       8
#define RGB_REMAP_IDX_ZL      9
#define RGB_REMAP_IDX_ZR      10 
#define RGB_REMAP_IDX_HOME    11
#define RGB_REMAP_IDX_CAPTURE 12 
#define RGB_REMAP_IDX_PLUS    13
#define RGB_REMAP_IDX_MINUS   14

#define FADES_TOTAL 24
uint32_t _color_fades[FADES_TOTAL] = {0};
uint32_t _color_fade_step = 0;
uint8_t _rgb_react_group_leds[HOJA_RGB_GROUPS_NUM][RGB_MAX_LEDS_PER_GROUP] = HOJA_RGB_GROUPINGS;
rgb_s color_black = {.color = 0x00};

#if defined(RGB_REACT_GROUP_ASSIGNMENT)
const int8_t _group_assignments[] = RGB_REACT_GROUP_ASSIGNMENT;
#else 
 #warning "RGB_REACT_GROUP_ASSIGNMENT not defined, reactive mode will not work"
#endif

static inline void _handle_button(
    bool button_state, 
    const uint8_t group_idx,
    uint8_t fade_idx,
    rgb_s *output )
{
    #if defined(HOJA_RGB_PLAYER_GROUP_IDX)
    if(group_idx == HOJA_RGB_PLAYER_GROUP_IDX) return;
    #endif

    // Control our fades
    if(button_state)
    {
        _color_fades[fade_idx] = 0;
    }
    else 
    {
        _color_fades[fade_idx] += _color_fade_step;
        if(_color_fades[fade_idx] > RGB_FADE_FIXED_MULT)
        {
            _color_fades[fade_idx] = RGB_FADE_FIXED_MULT;
        }
    }

    // Obtain our group color
    rgb_s group_color = rgb_colors_safe[group_idx];
    rgb_s blend = {.color = anm_utility_blend(&group_color, &color_black, _color_fades[fade_idx])};

    // Write color to output according to group
    for(int i = 0; i < RGB_MAX_LEDS_PER_GROUP; i++)
    {
        uint8_t idx_out = _rgb_react_group_leds[group_idx][i];
        if(i && !idx_out)
        {
            continue;
        }
        else
        {
            output[idx_out].color = blend.color;
        }
    }
}

static inline void _handle_dpad(
    bool button_state, 
    const uint8_t group_idx,
    const uint8_t dpad_idx,
    uint8_t fade_idx,
    rgb_s *output )
{
    // Control our fades
    if(button_state)
    {
        _color_fades[fade_idx] = 0;
    }
    else 
    {
        _color_fades[fade_idx] += _color_fade_step;
        if(_color_fades[fade_idx] > RGB_FADE_FIXED_MULT)
        {
            _color_fades[fade_idx] = RGB_FADE_FIXED_MULT;
        }
    }

    // Obtain our group color
    rgb_s group_color = rgb_colors_safe[group_idx];
    rgb_s blend = {.color = anm_utility_blend(&group_color, &color_black, _color_fades[fade_idx])};

    uint8_t idx_out = _rgb_react_group_leds[group_idx][dpad_idx];
    output[idx_out].color = blend.color;
}

bool anm_react_get_state(rgb_s *output)
{
    _color_fade_step = anm_utility_get_time_fixedpoint();
    // Reset our static state from stored rgb group data
    for(int i = 0; i < FADES_TOTAL; i++)
    {
        _color_fades[i] = RGB_FADE_FIXED_MULT;
    }

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        output[i].color = 0x00;
    }

    // Color player LEDs if they exist to our preset group color
    #if defined(HOJA_RGB_PLAYER_GROUP_IDX)
    for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
    {
        uint8_t idx = rgb_led_groups[HOJA_RGB_PLAYER_GROUP_IDX][i];
        output[idx] = rgb_colors_safe[HOJA_RGB_PLAYER_GROUP_IDX];
    }
    #endif

    return true;
}

bool anm_react_handler(rgb_s* output)
{
    static analog_data_s analog = {0};

    analog_access_safe(&analog,  ANALOG_ACCESS_SCALED_DATA);
    

}

#endif