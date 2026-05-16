#include "sinput_lib.h"
#include "sinput_lib_config.h"
#include "sinput_lib_protocol.h"

__attribute__((weak)) void sinput_api_hook_set_rumble(sinput_stereo_rumble_s rumble)
{
    (void)rumble;
}

__attribute__((weak)) void sinput_api_hook_set_haptics(sinput_stereo_haptics_s haptics)
{
    (void)haptics;
}

__attribute__((weak)) void sinput_api_hook_set_player_leds(uint8_t player_number)
{
    (void)player_number;
}

__attribute__((weak)) void sinput_api_hook_set_joystick_rgb(uint32_t rgb_value)
{
    (void)rgb_value;
}

__attribute__((weak)) bool sinput_api_hook_get_power(sinput_power_s *status)
{
    return false;
}

__attribute__((weak)) bool sinput_api_hook_get_buttons(sinput_buttons_s *out)
{
    return false;
}

__attribute__((weak)) bool sinput_api_hook_get_joysticks(sinput_joysticks_s *out)
{
    return false;
}

__attribute__((weak)) bool sinput_api_hook_get_triggers(sinput_triggers_s *out)
{
    return false;
}

__attribute__((weak)) bool sinput_api_hook_get_motion(sinput_motion_s *out)
{
    return false;
}

__attribute__((weak)) bool sinput_api_hook_get_touchpads(sinput_touchpads_s *out)
{
    return false;
}

bool sinput_api_generate_inputreport(uint8_t out[64])
{
    if(!out) return false;

    return sinput_protocol_generate_inputreport(out);
}

void sinput_api_output_tunnel(const uint8_t *data, uint16_t len)
{
    sinput_protocol_output_tunnel(data, len);
}

sinput_config_status_t sinput_api_init(sinput_device_cfg_s *cfg)
{
    return sinput_config_set(cfg);
}