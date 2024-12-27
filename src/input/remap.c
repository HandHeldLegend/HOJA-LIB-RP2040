#include "input/remap.h"
#include "input/button.h"
#include "input/triggers.h"
#include "hoja_shared_types.h"
#include "utilities/settings.h"
#include "hoja.h"

#include <string.h>

#define REMAP_SET(button, shift, unset) ( (unset) ? 0 : ((button) << shift))

button_remap_s  _remap_profile;
buttons_unset_s _unset_profile;

int _ltrigger_processing_mode = 0;
int _rtrigger_processing_mode = 0;

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
  button_data_s in_copy = {0};
  memcpy(&in_copy, in, sizeof(button_data_s));

  // Pre-process triggers
  triggers_process_pre(
    _ltrigger_processing_mode, 
    _rtrigger_processing_mode,
    &in_copy
    );

  uint16_t output = 0;

  output |= REMAP_SET(in_copy.button_a, _remap_profile.button_a, _unset_profile.button_a);
  output |= REMAP_SET(in_copy.button_b, _remap_profile.button_b, _unset_profile.button_b);
  output |= REMAP_SET(in_copy.button_x, _remap_profile.button_x, _unset_profile.button_x);
  output |= REMAP_SET(in_copy.button_y, _remap_profile.button_y, _unset_profile.button_y);

  output |= REMAP_SET(in_copy.dpad_up,    _remap_profile.dpad_up,     _unset_profile.dpad_up);
  output |= REMAP_SET(in_copy.dpad_left,  _remap_profile.dpad_left,   _unset_profile.dpad_left);
  output |= REMAP_SET(in_copy.dpad_down,  _remap_profile.dpad_down,   _unset_profile.dpad_down);
  output |= REMAP_SET(in_copy.dpad_right, _remap_profile.dpad_right,  _unset_profile.dpad_right);

  output |= REMAP_SET(in_copy.trigger_l,  _remap_profile.trigger_l,   _unset_profile.trigger_l);
  output |= REMAP_SET(in_copy.trigger_r,  _remap_profile.trigger_r,   _unset_profile.trigger_r);

  output |= REMAP_SET(in_copy.trigger_zl, _remap_profile.trigger_zl,  _unset_profile.trigger_zl);
  output |= REMAP_SET(in_copy.trigger_zr, _remap_profile.trigger_zr,  _unset_profile.trigger_zr);

  output |= REMAP_SET(in_copy.button_stick_left,  _remap_profile.button_stick_left,   _unset_profile.button_stick_left);
  output |= REMAP_SET(in_copy.button_stick_right, _remap_profile.button_stick_right,  _unset_profile.button_stick_right);

  // Set all output (remappable buttons only) in a single operation
  in_copy.buttons_all = output;

  triggers_process_post(
    _ltrigger_processing_mode, 
    _rtrigger_processing_mode,
    &in_copy
    );

  // Copy to output
  memcpy(out, &in_copy, sizeof(button_data_s));
}

void _remap_pack_remap(button_remap_s *remap, uint8_t profile_idx)
{
  uint32_t val_lower = (uint32_t) (remap->val & 0xFFFFFFFF);
  uint32_t val_upper = (uint32_t) ((remap->val & 0xFFFFFFFF00000000) >> 32);

  remap_config->profiles_lower[profile_idx] = val_lower;
  remap_config->profiles_upper[profile_idx] = val_upper;
}

void _remap_unpack_remap(uint8_t profile_idx, button_remap_s *remap)
{
  remap->val = ((uint64_t) remap_config->profiles_upper[profile_idx] << 32) | 
               ((uint64_t) remap_config->profiles_lower[profile_idx]);
}

void _remap_load_remap()
{
  uint8_t profile_idx = 0;
  uint8_t mode = hoja_gamepad_mode_get();
  switch(mode)
  {
    default:
    case GAMEPAD_MODE_SWPRO:
      // Profile 0
      break;

    case GAMEPAD_MODE_XINPUT:
      profile_idx = 1;
      break;

    case GAMEPAD_MODE_SNES:
      profile_idx = 2;
      break;

    case GAMEPAD_MODE_N64:
      profile_idx = 3;
      break;

    case GAMEPAD_MODE_GCUSB:
    case GAMEPAD_MODE_GAMECUBE:
      profile_idx = 4;
      break;
  }

  _remap_unpack_remap(profile_idx, &_remap_profile);
  _unset_profile.val = remap_config->disabled[profile_idx];

  // Does our ZL mapping match ZL?
  if(_remap_profile.trigger_zl == MAPCODE_T_ZL)
    _ltrigger_processing_mode = 0;
  else _rtrigger_processing_mode = 1;
  
  if(_remap_profile.trigger_zl == MAPCODE_T_ZR)
    _rtrigger_processing_mode = 0;
  else _rtrigger_processing_mode = 1;

  // Custom mode for GameCube modes
  // But only if we are mapped to original outputs
  switch(mode)
  {
    case GAMEPAD_MODE_GAMECUBE:
    case GAMEPAD_MODE_GCUSB:
      _rtrigger_processing_mode = (!_ltrigger_processing_mode) ? 2 : 0;
      _ltrigger_processing_mode = (!_rtrigger_processing_mode) ? 2 : 0;
    break;
  }
  
}

void remap_config_cmd(remap_cmd_t cmd, const uint8_t *data, setting_callback_t cb)
{
    const uint8_t cb_dat[3] = {CFG_BLOCK_REMAP, cmd, 0};

    switch(cmd)
    {
        default:
        break;

        case REMAP_CMD_REFRESH:
        break;
    }

    if(cb!=NULL)
    {
        cb(cb_dat, 3);
    }
}

void remap_reset_default(gamepad_mode_t mode)
{

}

void remap_init()
{

}

