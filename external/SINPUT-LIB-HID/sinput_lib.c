#include "sinput_lib.h"

void sinput_api_hook_set_rumble(sinput_stereo_rumble_s *rumble);
void sinput_api_hook_set_haptics(sinput_stereo_haptics_s *haptics);
void sinput_api_hook_set_player_leds(uint8_t player_number);
void sinput_api_hook_set_joystick_rgb(uint32_t rgb_value);

bool sinput_api_hook_get_power(sinput_power_s *status);
bool sinput_api_hook_get_buttons(sinput_buttons_s *out);
bool sinput_api_hook_get_joysticks(sinput_joysticks_s *out);
bool sinput_api_hook_get_triggers(sinput_triggers_s *out);
bool sinput_api_hook_get_motion(sinput_motion_s *out);
bool sinput_api_hook_get_touchpads(sinput_touchpads_s *out);

bool sinput_api_generate_inputreport(uint8_t out[64]);
void sinput_api_output_tunnel(const uint8_t *data, uint16_t len);

sinput_config_status_t sinput_api_init(sinput_device_cfg_s *cfg)
{

}