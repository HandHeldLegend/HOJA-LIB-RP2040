#include "usb/xinput.h"
#include "usb/xinput_usbd.h"
#include "input_shared_types.h"

short sign_axis(int input_axis)
{
    if (input_axis > 4095) {
        input_axis = 4095;  // Clamp to max 12-bit value
    }
    
    // Convert from [0, 4095] to [-2048, 2047] range
    int16_t centered = (int16_t)input_axis - 2048;
    
    // Scale from [-2048, 2047] to [-32768, 32767] range
    int32_t scaled = (int32_t)centered * 32768 / 2047;
    
    // Clamp to int16_t range to handle rounding issues
    if (scaled > 32767) {
        scaled = 32767;
    } else if (scaled < -32768) {
        scaled = -32768;
    }
    
    return (short)scaled;
}

void xinput_hid_report(uint32_t timestamp)
{
  static xid_input_s data = {0};
  static analog_data_s analog_data = {0};
  static button_data_s button_data = {0};

  data.stick_left_x     = sign_axis((int) (analog_data.lx));
  data.stick_left_y     = sign_axis((int) (analog_data.ly));
  data.stick_right_x    = sign_axis((int) (analog_data.rx));
  data.stick_right_y    = sign_axis((int) (analog_data.ry));

  data.dpad_up      = button_data.dpad_up;
  data.dpad_down    = button_data.dpad_down;
  data.dpad_left    = button_data.dpad_left;
  data.dpad_right   = button_data.dpad_right;

  data.button_guide = button_data.button_home;
  data.button_back = button_data.button_minus;
  data.button_menu = button_data.button_plus;
  data.bumper_r = button_data.trigger_r;
  data.bumper_l = button_data.trigger_l;

  data.button_a = button_data.button_a;
  data.button_b = button_data.button_b;
  data.button_x = button_data.button_x;
  data.button_y = button_data.button_y;

  data.button_stick_l = button_data.button_stick_left;
  data.button_stick_r = button_data.button_stick_right;

  data.analog_trigger_l = button_data.trigger_zl ? 255 : (button_data.zl_analog>>4);
  data.analog_trigger_r = button_data.trigger_zr ? 255 : (button_data.zr_analog>>4);

  tud_xinput_report(&data, XID_REPORT_LEN);
}
