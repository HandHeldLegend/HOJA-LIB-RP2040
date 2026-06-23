#include "devices/animations/anm_authentic.h"
#include "devices/animations/anm_authentic_palettes.h"
#include "devices/rgb.h"
#include "devices/animations/anm_utility.h"
#include "devices/animations/rgb_modes.h"
#include "input/mapper.h"
#include "utilities/settings.h"
#include "hoja.h"
#include "board_config.h"

#include <string.h>

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

static rgb_s _group_colors[RGB_MAX_GROUPS];
static bool _cache_valid = false;

static bool _is_stick_physical_input(mapper_input_code_t code)
{
    return code == INPUT_CODE_LX_RIGHT || code == INPUT_CODE_RX_RIGHT;
}

static void _unpack_groups_to_leds(rgb_s *output)
{
    for(int i = 0; i < rgb_group_count; i++)
    {
        for(int j = 0; j < RGB_MAX_LEDS_PER_GROUP; j++)
        {
            int index_out = rgb_led_groups[i][j];
            if(index_out < 0)
                continue;
            output[index_out].color = _group_colors[i].color;
        }
    }
}

void anm_authentic_refresh(void)
{
    if(!rgb_config || rgb_config->rgb_mode != RGB_ANIM_AUTHENTIC)
        return;

    _cache_valid = false;
}

static void _resolve_group_colors(void)
{
    const hoja_config_s *cfg = hoja_config_get();
    if(!cfg)
    {
        for(int i = 0; i < RGB_MAX_GROUPS; i++)
            _group_colors[i] = anm_authentic_fallback_color();
        _cache_valid = true;
        return;
    }

    const hoja_rgb_cfg_s *rgb_cfg = &cfg->rgb;
    const inputConfigSlot_s *profile = mapper_get_active_profile();
    core_reportformat_t format = mapper_get_palette_format();
    rgb_s fallback = anm_authentic_fallback_color();
    rgb_s stick = anm_authentic_stick_color(format);

    for(int g = 0; g < RGB_MAX_GROUPS; g++)
        _group_colors[g] = fallback;

    for(int g = 0; g < rgb_group_count; g++)
    {
        if(rgb_cfg->notification_group_index >= 0
        && g == (uint8_t)rgb_cfg->notification_group_index)
        {
            rgb_s user = rgb_colors_safe[g];
            anm_utility_apply_color_safety(&user);
            _group_colors[g] = user;
            continue;
        }

        if(rgb_cfg->player_group_index >= 0
        && g == (uint8_t)rgb_cfg->player_group_index)
        {
            rgb_s user = rgb_colors_safe[g];
            anm_utility_apply_color_safety(&user);
            _group_colors[g] = user;
            continue;
        }

        mapper_input_code_t physical = INPUT_CODE_UNUSED;
        for(int s = 0; s < rgb_cfg->reactive_count && s < RGB_MAX_REACTIVE_SLOTS; s++)
        {
            if(rgb_cfg->reactive[s].group == (uint8_t)g)
            {
                physical = rgb_cfg->reactive[s].input;
                break;
            }
        }

        if(physical <= INPUT_CODE_UNUSED || physical >= INPUT_CODE_MAX)
            continue;

        if(_is_stick_physical_input(physical))
        {
            _group_colors[g] = stick;
            continue;
        }

        if(!profile)
            continue;

        int8_t output_code = profile[physical].output_code;
        if(output_code < 0)
        {
            _group_colors[g] = (rgb_s){ .r = 0, .g = 0, .b = 0 };
            continue;
        }

        rgb_s resolved = fallback;
        if(anm_authentic_palette_color(format, output_code, &resolved))
            _group_colors[g] = resolved;
    }

    _cache_valid = true;
}

static void _ensure_cache(void)
{
    if(!_cache_valid)
        _resolve_group_colors();
}

bool anm_authentic_get_state(rgb_s *output)
{
    _ensure_cache();
    _unpack_groups_to_leds(output);
    return true;
}

bool anm_authentic_handler(rgb_s *output)
{
    _ensure_cache();
    _unpack_groups_to_leds(output);
    return true;
}

#else

void anm_authentic_refresh(void)
{
}

#endif
