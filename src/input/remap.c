#include "input/remap.h"
#include "input/button.h"
#include "input/trigger.h"
#include "hoja_shared_types.h"
#include "utilities/settings.h"
#include "hoja.h"

#include <string.h>

#define REMAP_SET(inputButton, outputButton) ( (outputButton < 0) ? 0 : (inputButton << outputButton) )

buttonRemap_s   _remap_profile;

remap_trigger_t _ltrigger_type = REMAP_TRIGGER_MATCHING;
remap_trigger_t _rtrigger_type = REMAP_TRIGGER_MATCHING;

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

#define MAX_ANALOG_OUT 4095
#define DEFAULT_UNSET {.val=0x00}

const buttonRemap_s   _default_remap = DEFAULT_REMAP;

buttonRemap_s  _remap_profile = DEFAULT_REMAP;

// Runs before we remap anything in remap.c
void _trigger_remap_preprocess(button_data_s *state, trigger_data_s *triggers_in, remap_trigger_t left_type, remap_trigger_t right_type)
{
    switch(left_type)
    {
        default:
        break;

        case REMAP_TRIGGER_MISMATCH:
        state->trigger_zl |= triggers_in->left_hairpin;
        break;
    }

    switch(right_type)
    {
        default:
        break;

        case REMAP_TRIGGER_MISMATCH:
        state->trigger_zr |= triggers_in->right_hairpin;
        break;
    }
}

// Runs after our remap in remap.c
void _trigger_remap_postprocess(
  button_data_s *state, 
  trigger_data_s *triggers_in, 
  trigger_data_s *triggers_out, 
  remap_trigger_t left_type, 
  remap_trigger_t right_type)
{
  switch(left_type)
  {
      default:
      case REMAP_TRIGGER_MATCHING:
      triggers_out->left_analog     = triggers_in->left_analog;
      triggers_out->left_hairpin    = triggers_in->left_hairpin;
      break;

      case REMAP_TRIGGER_MISMATCH:
      triggers_out->left_analog     = state->trigger_zl ? 0 : MAX_ANALOG_OUT;
      triggers_out->left_hairpin    = state->trigger_zl;
      break;

      case REMAP_TRIGGER_SWAPPED:
      triggers_out->right_analog     = triggers_in->left_analog;
      triggers_out->right_hairpin    = triggers_in->left_hairpin;
      break;
  }

  switch(right_type)
  {
      default:
      case REMAP_TRIGGER_MATCHING:
      triggers_out->right_analog     = triggers_in->right_analog;
      triggers_out->right_hairpin    = triggers_in->right_hairpin;
      break;

      case REMAP_TRIGGER_MISMATCH:
      triggers_out->right_analog     = state->trigger_zr ? 0 : MAX_ANALOG_OUT;
      triggers_out->right_hairpin    = state->trigger_zr;
      break;

      case REMAP_TRIGGER_SWAPPED:
      triggers_out->left_analog     = triggers_in->right_analog;
      triggers_out->left_hairpin    = triggers_in->right_hairpin;
      break;
  }

  if(state->trigger_zl)
      triggers_out->left_analog  = MAX_ANALOG_OUT;

  if(state->trigger_zr)
      triggers_out->right_analog = MAX_ANALOG_OUT;
}

void remap_get_processed_input(button_data_s *buttons_out, trigger_data_s *triggers_out)
{
  static button_data_s  tmp_input_buttons  = {0};
  static trigger_data_s tmp_input_triggers = {0};

  static button_data_s  tmp_output_buttons  = {0};
  static trigger_data_s tmp_output_triggers = {0};

  // Obtain button data and trigger data
  button_access_safe(&tmp_input_buttons, BUTTON_ACCESS_RAW_DATA);
  trigger_access_safe(&tmp_input_triggers, TRIGGER_ACCESS_SCALED_DATA);

  uint16_t output = 0;

  _trigger_remap_preprocess(&tmp_input_buttons, &tmp_input_triggers, _ltrigger_type, _rtrigger_type);

  output |= REMAP_SET(tmp_input_buttons.button_a, _remap_profile.button_a);
  output |= REMAP_SET(tmp_input_buttons.button_b, _remap_profile.button_b);
  output |= REMAP_SET(tmp_input_buttons.button_x, _remap_profile.button_x);
  output |= REMAP_SET(tmp_input_buttons.button_y, _remap_profile.button_y);

  output |= REMAP_SET(tmp_input_buttons.dpad_up,    _remap_profile.dpad_up);
  output |= REMAP_SET(tmp_input_buttons.dpad_left,  _remap_profile.dpad_left);
  output |= REMAP_SET(tmp_input_buttons.dpad_down,  _remap_profile.dpad_down);
  output |= REMAP_SET(tmp_input_buttons.dpad_right, _remap_profile.dpad_right);

  output |= REMAP_SET(tmp_input_buttons.trigger_l,  _remap_profile.trigger_l);
  output |= REMAP_SET(tmp_input_buttons.trigger_r,  _remap_profile.trigger_r);

  output |= REMAP_SET(tmp_input_buttons.trigger_zl, _remap_profile.trigger_zl);
  output |= REMAP_SET(tmp_input_buttons.trigger_zr, _remap_profile.trigger_zr);

  output |= REMAP_SET(tmp_input_buttons.button_stick_left,  _remap_profile.button_stick_left);
  output |= REMAP_SET(tmp_input_buttons.button_stick_right, _remap_profile.button_stick_right);

  output |= REMAP_SET(tmp_input_buttons.button_plus,  _remap_profile.button_plus);
  output |= REMAP_SET(tmp_input_buttons.button_minus, _remap_profile.button_minus);

  tmp_output_buttons.buttons_all    = output;
  tmp_output_buttons.buttons_system = tmp_input_buttons.buttons_system;

  _trigger_remap_postprocess(&tmp_output_buttons, &tmp_input_triggers, &tmp_output_triggers, _ltrigger_type, _rtrigger_type);

  // Copy out
  memcpy(buttons_out,   &tmp_output_buttons,   sizeof(button_data_s));
  memcpy(triggers_out,  &tmp_output_triggers,  sizeof(trigger_data_s));
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
    _ltrigger_type = REMAP_TRIGGER_MATCHING;
  else if(_remap_profile.trigger_zl == MAPCODE_T_ZR)
    _ltrigger_type = REMAP_TRIGGER_SWAPPED;
  else 
    _ltrigger_type = REMAP_TRIGGER_MISMATCH;

  if(_remap_profile.trigger_zr == MAPCODE_T_ZR)
    _rtrigger_type = REMAP_TRIGGER_MATCHING;
  else if(_remap_profile.trigger_zr == MAPCODE_T_ZL)
    _rtrigger_type = REMAP_TRIGGER_SWAPPED;
  else 
    _rtrigger_type = REMAP_TRIGGER_MISMATCH;
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

