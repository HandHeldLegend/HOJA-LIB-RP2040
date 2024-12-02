#include "input/remap.h"
#include "input/button.h"
#include "hoja_shared_types.h"
#include "input_shared_types.h"
#include "hoja.h"

#define REMAP_SET(button, shift, unset) ( (unset) ? 0 : ((button) << shift))

button_remap_s  _remap_profile;
buttons_unset_s _unset_profile;

bool _l_analog_remapped = false;
bool _r_analog_remapped = false;

#define DEFAULT_REMAP { \
    .dpad_up = MAPCODE_DUP, \
    .dpad_down = MAPCODE_DDOWN, \
    .dpad_left = MAPCODE_DLEFT, \
    .dpad_right = MAPCODE_DRIGHT, \
    .button_a = MAPCODE_B_A, \
    .button_b = MAPCODE_B_B, \
    .button_x = MAPCODE_B_X, \
    .button_y = MAPCODE_B_Y, \
    .trigger_l = MAPCODE_T_L, \
    .trigger_r = MAPCODE_T_R, \
    .trigger_zl = MAPCODE_T_ZL, \
    .trigger_zr = MAPCODE_T_ZR, \
    .button_plus = MAPCODE_B_PLUS, \
    .button_minus = MAPCODE_B_MINUS, \
    .button_stick_left = MAPCODE_B_STICKL, \
    .button_stick_right = MAPCODE_B_STICKR \
}

#define DEFAULT_UNSET {.val=0x00}

const button_remap_s  _default_remap = DEFAULT_REMAP;
const buttons_unset_s _default_unset = DEFAULT_UNSET;

const buttons_unset_s _default_n64_unset       = {.val = (1<<MAPCODE_B_STICKL) | (1<<MAPCODE_B_STICKR)};
const buttons_unset_s _default_gamecube_unset  = {.val = (1<<MAPCODE_B_MINUS) | (1<<MAPCODE_B_STICKL) | (1<<MAPCODE_B_STICKR) | (1<<MAPCODE_T_L)};
const buttons_unset_s _default_snes_unset      = {.val = (1<<MAPCODE_B_STICKL) | (1<<MAPCODE_B_STICKR) | (1<<MAPCODE_T_ZL) | (1<<MAPCODE_T_ZR)};

button_remap_s  _remap_profile = DEFAULT_REMAP;
buttons_unset_s _unset_profile = DEFAULT_UNSET;

void remap_process(button_data_s *in, button_data_s *out)
{
  uint16_t output = 0;

  output |= REMAP_SET(in->button_a, _remap_profile.button_a, _unset_profile.button_a);
  output |= REMAP_SET(in->button_b, _remap_profile.button_b, _unset_profile.button_b);
  output |= REMAP_SET(in->button_x, _remap_profile.button_x, _unset_profile.button_x);
  output |= REMAP_SET(in->button_y, _remap_profile.button_y, _unset_profile.button_y);

  output |= REMAP_SET(in->dpad_up,    _remap_profile.button_a,    _unset_profile.button_a);
  output |= REMAP_SET(in->dpad_left,  _remap_profile.dpad_left,   _unset_profile.dpad_left);
  output |= REMAP_SET(in->dpad_down,  _remap_profile.dpad_down,   _unset_profile.dpad_down);
  output |= REMAP_SET(in->dpad_right, _remap_profile.dpad_right,  _unset_profile.dpad_right);

  output |= REMAP_SET(in->trigger_l,  _remap_profile.trigger_l,   _unset_profile.trigger_l);
  output |= REMAP_SET(in->trigger_r,  _remap_profile.trigger_r,   _unset_profile.trigger_r);
  output |= REMAP_SET(in->trigger_zl, _remap_profile.trigger_zl,  _unset_profile.trigger_zl);
  output |= REMAP_SET(in->trigger_zr, _remap_profile.trigger_zr,  _unset_profile.trigger_zr);

  output |= REMAP_SET(in->button_stick_left,  _remap_profile.button_stick_left,   _unset_profile.button_stick_left);
  output |= REMAP_SET(in->button_stick_right, _remap_profile.button_stick_right,  _unset_profile.button_stick_right);

  // Set all output in a single operation
  out->buttons_all = output;
}

void _remap_load_remap()
{
  switch(hoja_gamepad_mode_get())
  {
    default:
    case GAMEPAD_MODE_SWPRO:
      //_remap_profile.val = global_loaded_settings.remap_switch.remap.val);
      //*unset_out = &(global_loaded_settings.remap_switch.disabled);
      break;

    case GAMEPAD_MODE_GCUSB:
    case GAMEPAD_MODE_GAMECUBE:
      //_remap_profile.val = &(global_loaded_settings.remap_gamecube.remap);
      //*unset_out = &(global_loaded_settings.remap_gamecube.disabled);
      break;

    case GAMEPAD_MODE_XINPUT:
      //_remap_profile.val = &(global_loaded_settings.remap_xinput.remap);
      //*unset_out = &(global_loaded_settings.remap_xinput.disabled);
      break;

    case GAMEPAD_MODE_N64:
      //_remap_profile.val = &(global_loaded_settings.remap_n64.remap);
      //*unset_out = &(global_loaded_settings.remap_n64.disabled);
      break;

    case GAMEPAD_MODE_SNES:
      //_remap_profile.val = &(global_loaded_settings.remap_snes.remap);
      //*unset_out = &(global_loaded_settings.remap_snes.disabled);
      break;
  }
  
}

void _remap_pack_remap(button_remap_s *remap, mapcode_t *array)
{

}

void _remap_unpack_remap(mapcode_t *array, button_remap_s *remap)
{

}

void _remap_listener(uint16_t buttons, bool clear)
{

}

void remap_send_data_webusb(gamepad_mode_t mode)
{

}

void remap_reset_default(gamepad_mode_t mode)
{

}

void remap_init(gamepad_mode_t mode, button_data_s *in, button_data_s *out)
{

}

void remap_set_gc_sp(gc_sp_mode_t sp_mode)
{

}

void remap_listen_stop()
{

}

void remap_listen_enable(gamepad_mode_t mode, mapcode_t mapcode)
{

}
