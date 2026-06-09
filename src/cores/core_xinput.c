#include "cores/core_xinput.h"

#include <hoja_usb.h>
#include "cores/cores.h"
#include "transport/transport.h"

// USB device/config descriptors are owned by the HHL-TINYUSB-DRIVERS library
#include "hhl_tusb_xinput.h"

// Descriptor pointers are populated from the driver library at init time.
static core_hid_device_t _xinput_hid_device = {
    .config_descriptor          = NULL,
    .config_descriptor_len      = 0,
    .hid_report_descriptor      = NULL,
    .hid_report_descriptor_len  = 0,
    .device_descriptor          = NULL,
    .name = HHL_TUSB_XINPUT_NAME,
    .pid  = HHL_TUSB_XINPUT_PID,
    .vid  = HHL_TUSB_XINPUT_VID,
};

#define CORE_XINPUT_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

short _core_xinput_scale_axis(int16_t input_axis)
{   
    return CORE_XINPUT_CLAMP(input_axis * 16, INT16_MIN, INT16_MAX);
}

void _core_xinput_report_tunnel_cb(const uint8_t *data, uint16_t len)
{
    if(len<2) return;

    uint8_t report_id = data[0];

    if(report_id==0x00 && data[1]==0x08)
    {
        tp_evt_s rumble = {
            .evt = TP_EVT_ERMRUMBLE,
            .evt_ermrumble = {
            .left=data[3], .right=data[4],
            .leftbrake=!data[3]?true:false, .rightbrake=!data[4]?true:false
        }};

        transport_evt_cb(rumble);
    }
    else if (report_id==0x01 && data[1]==0x03)
    {
        tp_evt_s cevt = {
            .evt = TP_EVT_CONNECTIONCHANGE,
            .evt_connectionchange = {.connection=TP_CONNECTION_CONNECTED},
        };

        tp_evt_s evt = {
            .evt = TP_EVT_PLAYERLED,
            .evt_playernumber = {.player_number=0},
        };
        
        switch(data[2])
        {
            // Off
            case 0x00:
            evt.evt_playernumber.player_number = 0;
            break;

            // Blink
            case 0x01:
            
            // Rotating
            case 0x0A:
            // Blinking
            case 0x0B:
            // Slow Blinking
            case 0x0C:
            //Alternating
            case 0x0D:

            // 1-4 Flashes // On
            case 0x02 ... 0x05:
            cevt.evt_connectionchange.connection=TP_CONNECTION_DISCONNECTED;
            transport_evt_cb(cevt);
            break;

            // P1 - P4
            case 0x06 ... 0x09:
            evt.evt_playernumber.player_number = data[2]-0x05;
            transport_evt_cb(cevt);
            transport_evt_cb(evt);
            break;
            
        }
    }
}

bool _core_xinput_get_generated_report(core_report_s *out)
{
    out->reportformat = CORE_REPORTFORMAT_XINPUT;
    out->size=CORE_XINPUT_REPORT_LEN;

    core_xinput_report_s *data = (core_xinput_report_s*)out->data;

    mapper_input_s input = mapper_get_input();

    data->report_id   = CORE_XINPUT_REPORT_ID;
    data->report_size = CORE_XINPUT_REPORT_SIZE;

    data->stick_left_x  = _core_xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_LX_LEFT], input.inputs[XINPUT_CODE_LX_RIGHT]));
    data->stick_left_y  = _core_xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_LY_DOWN], input.inputs[XINPUT_CODE_LY_UP]));
    data->stick_right_x = _core_xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_RX_LEFT], input.inputs[XINPUT_CODE_RX_RIGHT]));
    data->stick_right_y = _core_xinput_scale_axis((int16_t) mapper_joystick_concat(0, input.inputs[XINPUT_CODE_RY_DOWN], input.inputs[XINPUT_CODE_RY_UP]));

    data->analog_trigger_l = CORE_XINPUT_CLAMP(input.inputs[XINPUT_CODE_LT_ANALOG], 0, 4095) >> 4;
    data->analog_trigger_r = CORE_XINPUT_CLAMP(input.inputs[XINPUT_CODE_RT_ANALOG], 0, 4095) >> 4;

    bool dpad[4] = {input.presses[XINPUT_CODE_DOWN], input.presses[XINPUT_CODE_RIGHT],
                
        input.presses[XINPUT_CODE_LEFT], input.presses[XINPUT_CODE_UP]};

    dpad_translate_input(dpad);

    data->dpad_down  = dpad[0];
    data->dpad_right = dpad[1];
    data->dpad_left  = dpad[2];
    data->dpad_up    = dpad[3];

    data->button_guide = input.presses[XINPUT_CODE_GUIDE];
    data->button_back = input.presses[XINPUT_CODE_BACK];
    data->button_menu = input.presses[XINPUT_CODE_START];
    data->bumper_r = input.presses[XINPUT_CODE_RB];
    data->bumper_l = input.presses[XINPUT_CODE_LB];

    data->button_a = input.presses[XINPUT_CODE_A];
    data->button_b = input.presses[XINPUT_CODE_B];
    data->button_x = input.presses[XINPUT_CODE_X];
    data->button_y = input.presses[XINPUT_CODE_Y];

    data->button_stick_l = input.presses[XINPUT_CODE_LS];
    data->button_stick_r = input.presses[XINPUT_CODE_RS];

    return true;
}

bool core_xinput_init(core_params_s *params)
{
    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_USB:
        _xinput_hid_device.device_descriptor     = (const hoja_usb_device_descriptor_t *)hhl_tusb_xinput_device_descriptor();
        _xinput_hid_device.config_descriptor     = hhl_tusb_xinput_configuration_descriptor();
        _xinput_hid_device.config_descriptor_len = hhl_tusb_xinput_configuration_descriptor_len();
        params->hid_device = &_xinput_hid_device;
        params->core_pollrate_us = 1000;
        break;
        
        default:
        return false;
    }

    params->core_report_format = CORE_REPORTFORMAT_XINPUT;
    params->core_report_generator = _core_xinput_get_generated_report;
    params->core_report_tunnel = _core_xinput_report_tunnel_cb;

    return transport_init(params);
}