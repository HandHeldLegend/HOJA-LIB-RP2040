#include "input/remap.h"
#include "input/button.h"
#include "input/triggers.h"
#include "hoja_shared_types.h"
#include "utilities/settings.h"
#include "hoja.h"

#include <string.h>

#define REMAP_SET(inputButton, outputButton) ( (outputButton < 0) ? 0 : (inputButton << outputButton) )

buttonRemap_s   _remap_profile;

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

const buttonRemap_s   _default_remap = DEFAULT_REMAP;

buttonRemap_s  _remap_profile = DEFAULT_REMAP;

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

  output |= REMAP_SET(in_copy.button_a, _remap_profile.button_a);
  output |= REMAP_SET(in_copy.button_b, _remap_profile.button_b);
  output |= REMAP_SET(in_copy.button_x, _remap_profile.button_x);
  output |= REMAP_SET(in_copy.button_y, _remap_profile.button_y);

  output |= REMAP_SET(in_copy.dpad_up,    _remap_profile.dpad_up);
  output |= REMAP_SET(in_copy.dpad_left,  _remap_profile.dpad_left);
  output |= REMAP_SET(in_copy.dpad_down,  _remap_profile.dpad_down);
  output |= REMAP_SET(in_copy.dpad_right, _remap_profile.dpad_right);

  output |= REMAP_SET(in_copy.trigger_l,  _remap_profile.trigger_l);
  output |= REMAP_SET(in_copy.trigger_r,  _remap_profile.trigger_r);

  output |= REMAP_SET(in_copy.trigger_zl, _remap_profile.trigger_zl);
  output |= REMAP_SET(in_copy.trigger_zr, _remap_profile.trigger_zr);

  output |= REMAP_SET(in_copy.button_stick_left,  _remap_profile.button_stick_left);
  output |= REMAP_SET(in_copy.button_stick_right, _remap_profile.button_stick_right);

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

void _remap_pack_remap(buttonRemap_s *remap, uint8_t profile_idx)
{
  memcpy(&remap_config->profiles[profile_idx], remap, BUTTON_REMAP_SIZE);
}

void _remap_unpack_remap(uint8_t profile_idx, buttonRemap_s *remap)
{
  memcpy(remap, &remap_config->profiles[profile_idx], BUTTON_REMAP_SIZE);
}

void _remap_load_remap()
{
  uint8_t profile_idx = 0;
  uint8_t mode = hoja_get_status().gamepad_mode;
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
  if(remap_config->remap_config_version != CFG_BLOCK_REMAP_VERSION)
  {
    remap_config->remap_config_version = CFG_BLOCK_REMAP_VERSION;
    for(int i = 0; i < 12; i++)
    {
      remap_config->profiles[i] = _default_remap;
    }
  }

  _remap_load_remap();
}

