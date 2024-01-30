#include "remap.h"

#define REMAP_SET(button, shift, unset) ( (!unset) ? ((button) << shift) : 0)

button_data_s *_buttons_in;
button_data_s *_buttons_out;

input_mode_t    _remap_mode;
button_remap_s *_remap_struct;
buttons_unset_s *_unset_struct;

mapcode_t _remap_arr[MAPCODE_MAX];


mapcode_t       _button_remap_code;
bool            _button_remap_listen = false;
mapcode_t       _tmp_remap_arr[MAPCODE_MAX];
button_remap_s  *_tmp_remap_struct;
buttons_unset_s *_tmp_unset_struct;
input_mode_t    _tmp_remap_mode;

bool _l_analog_remapped = false;
bool _r_analog_remapped = false;

const button_remap_s default_remap = {
    .dpad_up = MAPCODE_DUP,
    .dpad_down = MAPCODE_DDOWN,
    .dpad_left = MAPCODE_DLEFT,
    .dpad_right = MAPCODE_DRIGHT,

    .button_a = MAPCODE_B_A,
    .button_b = MAPCODE_B_B,
    .button_x = MAPCODE_B_X,
    .button_y = MAPCODE_B_Y,

    .trigger_l = MAPCODE_T_L,
    .trigger_r = MAPCODE_T_R,
    .trigger_zl = MAPCODE_T_ZL,
    .trigger_zr = MAPCODE_T_ZR,

    .button_plus = MAPCODE_B_PLUS,
    .button_minus = MAPCODE_B_MINUS,
    .button_stick_left = MAPCODE_B_STICKL,
    .button_stick_right = MAPCODE_B_STICKR,
};

const buttons_unset_s default_unset = {.val = 0x00};

const buttons_unset_s default_n64_unset       = {.val = (1<<MAPCODE_B_STICKL) | (1<<MAPCODE_B_STICKR)};
const buttons_unset_s default_gamecube_unset  = {.val = (1<<MAPCODE_B_MINUS) | (1<<MAPCODE_B_STICKL) | (1<<MAPCODE_B_STICKR) | (1<<MAPCODE_T_L)};
const buttons_unset_s default_snes_unset      = {.val = (1<<MAPCODE_B_STICKL) | (1<<MAPCODE_B_STICKR) | (1<<MAPCODE_T_ZL) | (1<<MAPCODE_T_ZR)};

void _remap_load_remap(input_mode_t mode, button_remap_s **remap_out, buttons_unset_s **unset_out)
{
  switch(mode)
  {
    default:
    case INPUT_MODE_SWPRO:
      *remap_out = &(global_loaded_settings.remap_switch.remap);
      *unset_out = &(global_loaded_settings.remap_switch.disabled);
      break;

    case INPUT_MODE_GCUSB:
    case INPUT_MODE_GAMECUBE:
      *remap_out = &(global_loaded_settings.remap_gamecube.remap);
      *unset_out = &(global_loaded_settings.remap_gamecube.disabled);
      break;

    case INPUT_MODE_XINPUT:
      *remap_out = &(global_loaded_settings.remap_xinput.remap);
      *unset_out = &(global_loaded_settings.remap_xinput.disabled);
      break;

    case INPUT_MODE_N64:
      *remap_out = &(global_loaded_settings.remap_n64.remap);
      *unset_out = &(global_loaded_settings.remap_n64.disabled);
      break;

    case INPUT_MODE_SNES:
      *remap_out = &(global_loaded_settings.remap_snes.remap);
      *unset_out = &(global_loaded_settings.remap_snes.disabled);
      break;
  }
  
}

void _remap_pack_remap(button_remap_s *remap, mapcode_t *array)
{
  remap->dpad_up    = array[MAPCODE_DUP];
  remap->dpad_down  = array[MAPCODE_DDOWN];
  remap->dpad_left  = array[MAPCODE_DLEFT];
  remap->dpad_right = array[MAPCODE_DRIGHT];
  remap->button_a   = array[MAPCODE_B_A];
  remap->button_b   = array[MAPCODE_B_B];
  remap->button_x   = array[MAPCODE_B_X];
  remap->button_y   = array[MAPCODE_B_Y];
  remap->trigger_l  = array[MAPCODE_T_L];
  remap->trigger_zl = array[MAPCODE_T_ZL];
  remap->trigger_r  = array[MAPCODE_T_R];
  remap->trigger_zr = array[MAPCODE_T_ZR];
  remap->button_plus        = array[MAPCODE_B_PLUS];
  remap->button_minus       = array[MAPCODE_B_MINUS];
  remap->button_stick_left  = array[MAPCODE_B_STICKL];
  remap->button_stick_right = array[MAPCODE_B_STICKR];
}

void _remap_unpack_remap(mapcode_t *array, button_remap_s *remap)
{
  array[MAPCODE_DUP]    = remap->dpad_up; 
  array[MAPCODE_DDOWN]  = remap->dpad_down;
  array[MAPCODE_DLEFT]  = remap->dpad_left;
  array[MAPCODE_DRIGHT] = remap->dpad_right;
  array[MAPCODE_B_A]    = remap->button_a;
  array[MAPCODE_B_B]    = remap->button_b;
  array[MAPCODE_B_X]    = remap->button_x;
  array[MAPCODE_B_Y]    = remap->button_y;
  array[MAPCODE_T_L]    = remap->trigger_l;
  array[MAPCODE_T_ZL]   = remap->trigger_zl;
  array[MAPCODE_T_R]    = remap->trigger_r;
  array[MAPCODE_T_ZR]   = remap->trigger_zr;
  array[MAPCODE_B_PLUS]   = remap->button_plus;
  array[MAPCODE_B_MINUS]  = remap->button_minus;    
  array[MAPCODE_B_STICKL] = remap->button_stick_left;
  array[MAPCODE_B_STICKR] = remap->button_stick_right;
}

void _remap_listener(uint16_t buttons, bool clear)
{
  /* Now we need to listen for which button has been pressed
   *  We do this by taking in the raw value and shifting
   *  right one bit at a time. If we have a bit set
   *  we know that we want to assign our set mapcode TO
   *  that button
   */
  mapcode_t output_button = 0;
  bool      got_map       = false;

  // Let's say our mapcode for _button_remap_code is L

  for (uint8_t i = 0; i < 16; i++)
  {
    if ((buttons >> i) & 0x1)
    {
      got_map = true;
      output_button = i;
      break;
    }
  }

  // Now we have the button that should ACTIVATE L

  if (got_map)
  {
    // First, we iterate through and make sure no other
    // buttons can output the same value.
    for(uint8_t i = 0; i < 16; i++)
    {
      if(_tmp_remap_arr[i] == _button_remap_code)
      {
        // If we have a match, we must unset the output
        _tmp_unset_struct->val |= (1<<i);
      }
    }

    // Set the output accordingly
    _tmp_remap_arr[output_button] = _button_remap_code;

    // Undo any unset that might exist on this output
    _tmp_unset_struct->val &= ~(1<<output_button);

    printf("Assigned %i to output %i\n", output_button, _button_remap_code);
    _button_remap_listen = false;
    _remap_pack_remap(_tmp_remap_struct, _tmp_remap_arr);
    remap_send_data_webusb(_tmp_remap_mode);
  }
  else if(clear)
  {
    for(uint8_t i = 0; i < 16; i++)
    {
      if(_tmp_remap_arr[i] == _button_remap_code)
      {
        printf("Disabled function for button %i\n", i);
        _tmp_unset_struct->val |= (1<<i);
        _button_remap_listen = false;
      }
    }
    _remap_pack_remap(_tmp_remap_struct, _tmp_remap_arr);
    remap_send_data_webusb(_tmp_remap_mode);
  }
}

void remap_send_data_webusb(input_mode_t mode)
{
  _remap_load_remap(mode, &_tmp_remap_struct, &_tmp_unset_struct);
  _remap_unpack_remap(_tmp_remap_arr, _tmp_remap_struct);
  remap_listen_stop();
  uint8_t b[64] = {0};
  b[0]  = WEBUSB_CMD_REMAP_GET;
  b[1]  = mode;
  b[2]  = _tmp_remap_struct->dpad_up;
  b[3]  = _tmp_remap_struct->dpad_down;
  b[4]  = _tmp_remap_struct->dpad_left;
  b[5]  = _tmp_remap_struct->dpad_right;
  b[6]  = _tmp_remap_struct->button_a;
  b[7]  = _tmp_remap_struct->button_b;
  b[8]  = _tmp_remap_struct->button_x;
  b[9]  = _tmp_remap_struct->button_y;
  b[10] = _tmp_remap_struct->trigger_l;
  b[11] = _tmp_remap_struct->trigger_zl;
  b[12] = _tmp_remap_struct->trigger_r;
  b[13] = _tmp_remap_struct->trigger_zr;
  b[14] = _tmp_remap_struct->button_plus;
  b[15] = _tmp_remap_struct->button_minus;
  b[16] = _tmp_remap_struct->button_stick_left;
  b[17] = _tmp_remap_struct->button_stick_right;
  b[18] = (_tmp_unset_struct->val & 0xFF00) >> 8;
  b[19] = (_tmp_unset_struct->val & 0xFF);

  // SEND OUTPUT FOR GC SP STUFF
  b[20] = global_loaded_settings.gc_sp_mode;
  b[21] = global_loaded_settings.gc_sp_light_trigger;
  
  webusb_ready_blocking(0);
  tud_vendor_n_flush(0);
  tud_vendor_n_write(0, b, 64);
}

void remap_reset_default(input_mode_t mode)
{
  _remap_load_remap(mode, &_remap_struct, &_unset_struct);

  _remap_struct->val = default_remap.val;

  switch(mode)
  {
    default:
      _unset_struct->val = default_unset.val;
      break;

    case INPUT_MODE_GAMECUBE:
      _unset_struct->val = default_gamecube_unset.val;
      break;
    
    case INPUT_MODE_N64:
      _unset_struct->val = default_n64_unset.val;
      break;

    case INPUT_MODE_SNES:
      _unset_struct->val = default_snes_unset.val;
      break;
  }

  _remap_unpack_remap(_remap_arr, _remap_struct);
}

void remap_init(input_mode_t mode, button_data_s *in, button_data_s *out)
{
  _buttons_in = in;
  _buttons_out = out;
  _remap_mode = mode;
  _remap_load_remap(mode, &_remap_struct, &_unset_struct);
  _remap_unpack_remap(_remap_arr, _remap_struct);

  _r_analog_remapped = !(_remap_arr[MAPCODE_T_ZR]==MAPCODE_T_ZR);
  _l_analog_remapped = !(_remap_arr[MAPCODE_T_ZL]==MAPCODE_T_ZL);
}

void remap_set_gc_sp(gc_sp_mode_t sp_mode)
{
  global_loaded_settings.gc_sp_mode = sp_mode;
}

void remap_listen_stop()
{
  _button_remap_listen = false;
  _remap_unpack_remap(_remap_arr, _remap_struct);
}

void remap_listen_enable(input_mode_t mode, mapcode_t mapcode)
{
  _remap_load_remap(mode, &_tmp_remap_struct, &_tmp_unset_struct);
  _remap_unpack_remap(_tmp_remap_arr, _tmp_remap_struct);
  /* Set the button we are assigning TO
   * EX we want to press a button that will activate
   * button A so we first input mapcode of MAPCODE_B_A */
  _tmp_remap_mode = mode;
  _button_remap_code = mapcode;
  _button_remap_listen = true;
}

void remap_buttons_task()
{
  _buttons_out->buttons_all = 0x00;
  _buttons_out->buttons_system = 0x00;

  if (_button_remap_listen)
  {
    bool c = _buttons_in->button_home;
    if(c) _buttons_in->button_home = 0;
    _remap_listener(_buttons_in->buttons_all, c);
  }

  if (macro_safe_mode_check())
  {
    _buttons_in->button_capture = 0;
    _buttons_in->button_home = 0;
    _buttons_in->button_plus = 0;
    _buttons_in->button_minus = 0;
    _buttons_in->dpad_up = 0;
    _buttons_in->dpad_down = 0;
    _buttons_in->dpad_left = 0;
    _buttons_in->dpad_right = 0;
  }

  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_plus, _remap_arr[MAPCODE_B_PLUS], _unset_struct->button_plus);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_minus, _remap_arr[MAPCODE_B_MINUS], _unset_struct->button_minus);

  _buttons_out->button_home     = _buttons_in->button_home;
  _buttons_out->button_capture  = _buttons_in->button_capture;

  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->dpad_up, _remap_arr[MAPCODE_DUP], _unset_struct->dpad_up);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->dpad_down, _remap_arr[MAPCODE_DDOWN], _unset_struct->dpad_down);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->dpad_left, _remap_arr[MAPCODE_DLEFT], _unset_struct->dpad_left);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->dpad_right, _remap_arr[MAPCODE_DRIGHT], _unset_struct->dpad_right);

  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_a, _remap_arr[MAPCODE_B_A], _unset_struct->button_a);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_b, _remap_arr[MAPCODE_B_B], _unset_struct->button_b);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_x, _remap_arr[MAPCODE_B_X], _unset_struct->button_x);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_y, _remap_arr[MAPCODE_B_Y], _unset_struct->button_y);

  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->trigger_l, _remap_arr[MAPCODE_T_L],    _unset_struct->trigger_l);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->trigger_r, _remap_arr[MAPCODE_T_R],    _unset_struct->trigger_r);

  #if(HOJA_CAPABILITY_ANALOG_TRIGGER_L)
    
    if(_l_analog_remapped)
    {
      bool l_triggered = (_buttons_in->zl_analog >= ANALOG_DIGITAL_THRESH) | _buttons_in->trigger_zl;
      _buttons_out->buttons_all |= REMAP_SET(l_triggered, _remap_arr[MAPCODE_T_ZL], _unset_struct->trigger_zl);
      _buttons_out->zl_analog = (_buttons_out->trigger_zl) ? 4095 : 0;
    }
    else
    {
      _buttons_out->zl_analog = _buttons_in->zl_analog;
      _buttons_out->trigger_zl = _buttons_in->trigger_zl;
    }
  #else
    _buttons_out->buttons_all |= REMAP_SET(_buttons_in->trigger_zl, _remap_arr[MAPCODE_T_ZL], _unset_struct->trigger_zl);
    _buttons_out->zl_analog = (_buttons_out->trigger_zl) ? 4095 : 0;
    _buttons_out->buttons_all |= REMAP_SET(_buttons_in->trigger_zl, _remap_arr[MAPCODE_T_ZL],  _unset_struct->trigger_zl);
  #endif

  #if(HOJA_CAPABILITY_ANALOG_TRIGGER_R)
    
    if(_r_analog_remapped)
    {
      bool r_triggered = (_buttons_in->zr_analog >= ANALOG_DIGITAL_THRESH) | _buttons_in->trigger_zr;
      _buttons_out->buttons_all |= REMAP_SET(r_triggered, _remap_arr[MAPCODE_T_ZR], _unset_struct->trigger_zr);
      _buttons_out->zr_analog = (_buttons_out->trigger_zr) ? 4095 : 0;
    }
    else
    {
      _buttons_out->zr_analog = _buttons_in->zr_analog;
      _buttons_out->trigger_zr = _buttons_in->trigger_zr;
    }
  #else
    _buttons_out->buttons_all |= REMAP_SET(_buttons_in->trigger_zr, _remap_arr[MAPCODE_T_ZR], _unset_struct->trigger_zr);
    _buttons_out->zr_analog = (_buttons_out->trigger_zr) ? 4095 : 0;
    _buttons_out->buttons_all |= REMAP_SET(_buttons_in->trigger_zr, _remap_arr[MAPCODE_T_ZR],  _unset_struct->trigger_zr);
  #endif

  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_stick_left, _remap_arr[MAPCODE_B_STICKL],   _unset_struct->button_stick_left);
  _buttons_out->buttons_all |= REMAP_SET(_buttons_in->button_stick_right, _remap_arr[MAPCODE_B_STICKR],  _unset_struct->button_stick_right);
}