#include "usb/xinput.h"
#include "usb/xinput_usbd.h"
#include "input_shared_types.h"

#include "input/analog.h"
#include "input/button.h"
#include "input/trigger.h"
#include "input/remap.h"

#define XINPUT_SHIFT_SCALE_BITS 16
#define XINPUT_SHIFT_SCALE_MULT (1<<XINPUT_SHIFT_SCALE_BITS)
#define XINPUT_FIXED_SCALER     ((32768.0f / 2047.0f) * XINPUT_SHIFT_SCALE_MULT)

short sign_axis(int16_t input_axis)
{   
    // Scale up to x16 (2048 to 32768 range)
    return (short)(input_axis * 16);
}

void xinput_hid_report(uint32_t timestamp)
{
    static xid_input_s data = {0};
    static analog_data_s analog_data = {0};
    static button_data_s button_data = {0};
    static trigger_data_s trigger_data = {0};

    analog_access_safe(&analog_data, ANALOG_ACCESS_DEADZONE_DATA);
    remap_get_processed_input(&button_data, &trigger_data);

    data.report_size = 20;

    data.stick_left_x     = sign_axis(analog_data.lx);
    data.stick_left_y     = sign_axis(analog_data.ly);
    data.stick_right_x    = sign_axis(analog_data.rx);
    data.stick_right_y    = sign_axis(analog_data.ry);

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

    data.analog_trigger_l = trigger_data.left_analog    >>  4;
    data.analog_trigger_r = trigger_data.right_analog   >>  4;

    tud_xinput_report(&data, XID_REPORT_LEN);
}
