
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "sinput_lib_config.h"

static sinput_device_cfg_s s_sinput_device_config;
static uint8_t s_sinput_config_ready;

sinput_config_status_t sinput_config_set(const sinput_device_cfg_s *cfg)
{
    memcpy(&s_sinput_device_config, cfg, sizeof(s_sinput_device_config));
    bool sanitization_error = false;

    // Sanitize config from error states
    if(s_sinput_device_config.polling_rate_us < 125)
    {
        // Default to 8ms
        s_sinput_device_config.polling_rate_us = 8000;
        sanitization_error |= true;
    }

    if(s_sinput_device_config.gamepad_format >= SINPUT_GAMEPAD_FORMAT_COUNT)
    {
        s_sinput_device_config.gamepad_format = SINPUT_GAMEPAD_FORMAT_JOYPAD;
        sanitization_error |= true;
    }

    if(s_sinput_device_config.gamepad_type >= SINPUT_SDL_GAMEPAD_TYPE_COUNT)
    {
        s_sinput_device_config.gamepad_type = SINPUT_SDL_GAMEPAD_TYPE_STANDARD;
        sanitization_error |= true;
    }

    if(s_sinput_device_config.face_buttons_style >= SINPUT_SDL_GAMEPAD_FACE_STYLE_COUNT)
    {
        s_sinput_device_config.face_buttons_style = SINPUT_SDL_GAMEPAD_FACE_STYLE_UNKNOWN;
        sanitization_error |= true;
    }

    s_sinput_config_ready = 1u;
    return sanitization_error ? SINPUT_CONFIG_ISSUES_OVERRIDE : SINPUT_CONFIG_OK;
}

void sinput_config_get(sinput_device_cfg_s *out)
{
    if(out == NULL) return;

    if(!s_sinput_config_ready) return;

    memcpy(out, &s_sinput_device_config, sizeof(s_sinput_device_config));
}
