#include "usb/xinput.h"
#include "usb/xinput_usbd.h"
#include "input_shared_types.h"

#include "input/analog.h"

#include "input/mapper.h"

#define XINPUT_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

short xinput_scale_axis(int16_t input_axis)
{   
    return XINPUT_CLAMP(input_axis * 16, INT16_MIN, INT16_MAX);
}

void xinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb)
{
    static xid_input_s data = {0};
    mapper_input_s input = mapper_get_input();

    data.report_size = 20;

    data.stick_left_x  = xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_LX_LEFT], input.inputs[XINPUT_CODE_LX_RIGHT]));
    data.stick_left_y  = xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_LY_DOWN], input.inputs[XINPUT_CODE_LY_UP]));
    data.stick_right_x = xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_RX_LEFT], input.inputs[XINPUT_CODE_RX_RIGHT]));
    data.stick_right_y = xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_RY_DOWN], input.inputs[XINPUT_CODE_RY_UP]));

    data.analog_trigger_l = XINPUT_CLAMP(input.inputs[XINPUT_CODE_LT_ANALOG], 0, 4095) >> 4;
    data.analog_trigger_r = XINPUT_CLAMP(input.inputs[XINPUT_CODE_RT_ANALOG], 0, 4095) >> 4;

    data.dpad_up      = input.presses[XINPUT_CODE_UP];
    data.dpad_down    = input.presses[XINPUT_CODE_DOWN];
    data.dpad_left    = input.presses[XINPUT_CODE_LEFT];
    data.dpad_right   = input.presses[XINPUT_CODE_RIGHT];

    data.button_guide = input.presses[XINPUT_CODE_GUIDE];
    data.button_back = input.presses[XINPUT_CODE_BACK];
    data.button_menu = input.presses[XINPUT_CODE_START];
    data.bumper_r = input.presses[XINPUT_CODE_RB];
    data.bumper_l = input.presses[XINPUT_CODE_LB];

    data.button_a = input.presses[XINPUT_CODE_A];
    data.button_b = input.presses[XINPUT_CODE_B];
    data.button_x = input.presses[XINPUT_CODE_X];
    data.button_y = input.presses[XINPUT_CODE_Y];

    data.button_stick_l = input.presses[XINPUT_CODE_LS];
    data.button_stick_r = input.presses[XINPUT_CODE_RS];

    tud_xinput_report(&data, XID_REPORT_LEN);
}
