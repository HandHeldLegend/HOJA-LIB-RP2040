#include "cores/core_xinput.h"

#include <hoja_usb.h>
#include "cores/cores.h"
#include "transport/transport.h"

const hoja_usb_device_descriptor_t _xinput_device_descriptor ={
    .bLength = sizeof(hoja_usb_device_descriptor_t),
    .bDescriptorType = HUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xFF,
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 =
        64,

    .idVendor = 0x045E,
    .idProduct = 0x028E,
    .bcdDevice = 0x0572,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01
};

#define XINPUT_CONFIG_DESCRIPTOR_LEN 41
const uint8_t _xinput_configuration_descriptor[48] = {
    0x09,       // bLength
    0x02,       // bDescriptorType (Configuration)
    0x30, 0x00, // wTotalLength 48
    0x01,       // bNumInterfaces 1
    0x01,       // bConfigurationValue
    0x00,       // iConfiguration (String Index)
    0x80,       // bmAttributes
    0xFA,       // bMaxPower 500mA

    0x09, // bLength
    0x04, // bDescriptorType (Interface)
    0x00, // bInterfaceNumber 0
    0x00, // bAlternateSetting
    0x02, // bNumEndpoints 2
    0xFF, // bInterfaceClass
    0x5D, // bInterfaceSubClass
    0x01, // bInterfaceProtocol
    0x00, // iInterface (String Index)

    0x10,       // bLength
    0x21,       // bDescriptorType (HID)
    0x10, 0x01, // bcdHID 1.10
    0x01,       // bCountryCode
    0x24,       // bNumDescriptors
    0x81,       // bDescriptorType[0] (Unknown 0x81)
    0x14, 0x03, // wDescriptorLength[0] 788
    0x00,       // bDescriptorType[1] (Unknown 0x00)
    0x03, 0x13, // wDescriptorLength[1] 4867
    0x02,       // bDescriptorType[2] (Unknown 0x02)
    0x00, 0x03, // wDescriptorLength[2] 768
    0x00,       // bDescriptorType[3] (Unknown 0x00)

    0x07,       // bLength
    0x05,       // bDescriptorType (Endpoint)
    0x81,       // bEndpointAddress (IN/D2H)
    0x03,       // bmAttributes (Interrupt)
    0x20, 0x00, // wMaxPacketSize 32
    0x01,       // bInterval 4 (unit depends on device speed)

    0x07,       // bLength
    0x05,       // bDescriptorType (Endpoint)
    0x02,       // bEndpointAddress (OUT/H2D)
    0x03,       // bmAttributes (Interrupt)
    0x20, 0x00, // wMaxPacketSize 32
    0x01,       // bInterval 8 (unit depends on device speed)
};

const core_hid_device_t _xinput_hid_device = {
    .config_descriptor      = _xinput_configuration_descriptor,
    .config_descriptor_len  = XINPUT_CONFIG_DESCRIPTOR_LEN,
    // .hid_report_descriptor  = ,
    // .hid_report_descriptor_len = ,
    .device_descriptor      = &_xinput_device_descriptor,
};

#define CORE_XINPUT_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

short _core_xinput_scale_axis(int16_t input_axis)
{   
    return CORE_XINPUT_CLAMP(input_axis * 16, INT16_MIN, INT16_MAX);
}

void _core_xinput_report_tunnel_cb(uint8_t *data, uint16_t len)
{
    if(len<2) return;

    uint8_t report_id = data[0];
}

bool _core_xinput_get_generated_report(core_report_s *out)
{
    out->reportformat = CORE_REPORTFORMAT_XINPUT;
    out->size=CORE_XINPUT_REPORT_SIZE;

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

}