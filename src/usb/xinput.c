#include "usb/xinput.h"
#include "usb/xinput_usbd.h"
#include "input_shared_types.h"

#include "input/analog.h"
#include "input/button.h"
#include "input/trigger.h"
#include "input/mapper.h"

#define XINPUT_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

short xinput_scale_axis(int16_t input_axis)
{   
    return XINPUT_CLAMP(input_axis * 16, INT16_MIN, INT16_MAX);
}

void xinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb)
{
    static xid_input_s data = {0};
    mapper_input_s *input = mapper_get_input();

    data.report_size = 20;

    data.stick_left_x     = xinput_scale_axis(input->joysticks_combined[0]);
    data.stick_left_y     = xinput_scale_axis(input->joysticks_combined[1]);
    data.stick_right_x    = xinput_scale_axis(input->joysticks_combined[2]);
    data.stick_right_y    = xinput_scale_axis(input->joysticks_combined[3]);

    data.dpad_up      = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_UP);
    data.dpad_down    = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_DOWN);
    data.dpad_left    = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_LEFT);
    data.dpad_right   = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_RIGHT);

    data.button_guide = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_GUIDE);
    data.button_back = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_BACK);
    data.button_menu = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_START);
    data.bumper_r = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_RB);
    data.bumper_l = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_LB);

    data.button_a = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_A);
    data.button_b = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_B);
    data.button_x = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_X);
    data.button_y = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_Y);

    data.button_stick_l = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_LS);
    data.button_stick_r = MAPPER_BUTTON_DOWN(input->digital_inputs, XINPUT_CODE_RS);

    data.analog_trigger_l = XINPUT_CLAMP(input->triggers[0] >>  4, 0, 255);
    data.analog_trigger_r = XINPUT_CLAMP(input->triggers[1] >>  4, 0, 255);

    tud_xinput_report(&data, XID_REPORT_LEN);
}
