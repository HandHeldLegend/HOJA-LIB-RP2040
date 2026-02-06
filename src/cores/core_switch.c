#include <string.h>

#include "cores/core_switch.h"

#include "switch/switch_commands.h"
#include "switch/switch_haptics.h"

#include "transport/transport.h"
#include "hoja_shared_types.h"

#include "hoja_usb.h"

#define REPORT_ID_SWITCH_INPUT 0x30
#define REPORT_ID_SWITCH_CMD 0x21
#define REPORT_ID_SWITCH_INIT 0x81

#define CORE_SWITCH_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#define NS_CMD_DATA_LEN 64
uint8_t _scmd[NS_CMD_DATA_LEN] = {0};
uint8_t _switch_report_size = 64;

/** Switch PRO HID MODE **/
// 1. Device Descriptor
// 2. HID Report Descriptor
// 3. Configuration Descriptor
// 4. TinyUSB Config
/**--------------------------**/
const hoja_usb_device_descriptor_t _swpro_device_descriptor = {
    .bLength = sizeof(hoja_usb_device_descriptor_t),
    .bDescriptorType = HUSB_DESC_DEVICE,
    .bcdUSB = 0x0210, // Changed from 0x0200 to 2.1 for BOS & WebUSB
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = 0x057E,
    .idProduct = 0x2009,

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

const uint8_t _swpro_hid_report_descriptor_usb[203] = {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x15, 0x00, // Logical Minimum (0)

    0x09, 0x04, // Usage (Joystick)
    0xA1, 0x01, // Collection (Application)

    0x85, 0x30, //   Report ID (48)
    0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x01, //   Usage Minimum (0x01)
    0x29, 0x0A, //   Usage Maximum (0x0A)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x0A, //   Report Count (10)
    0x55, 0x00, //   Unit Exponent (0)
    0x65, 0x00, //   Unit (None)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x0B, //   Usage Minimum (0x0B)
    0x29, 0x0E, //   Usage Maximum (0x0E)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x04, //   Report Count (4)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x02, //   Report Count (2)
    0x81, 0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x0B, 0x01, 0x00, 0x01, 0x00, //   Usage (0x010001)
    0xA1, 0x00,                   //   Collection (Physical)
    0x0B, 0x30, 0x00, 0x01, 0x00, //     Usage (0x010030)
    0x0B, 0x31, 0x00, 0x01, 0x00, //     Usage (0x010031)
    0x0B, 0x32, 0x00, 0x01, 0x00, //     Usage (0x010032)
    0x0B, 0x35, 0x00, 0x01, 0x00, //     Usage (0x010035)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65534)
    0x75, 0x10,                   //     Report Size (16)
    0x95, 0x04,                   //     Report Count (4)
    0x81, 0x02,                   //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                         //   End Collection

    0x0B, 0x39, 0x00, 0x01, 0x00, //   Usage (0x010039)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x07,                   //   Logical Maximum (7)
    0x35, 0x00,                   //   Physical Minimum (0)
    0x46, 0x3B, 0x01,             //   Physical Maximum (315)
    0x65, 0x14,                   //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x0F,                   //   Usage Minimum (0x0F)
    0x29, 0x12,                   //   Usage Maximum (0x12)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x34,                   //   Report Count (52)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x85, 0x21,       //   Report ID (33)
    0x09, 0x01,       //   Usage (0x01)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x3F,       //   Report Count (63)
    0x81, 0x03,       //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x85, 0x81, //   Report ID (-127)
    0x09, 0x02, //   Usage (0x02)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x81, 0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x85, 0x01, //   Report ID (1)
    0x09, 0x03, //   Usage (0x03)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0x85, 0x10, //   Report ID (16)
    0x09, 0x04, //   Usage (0x04)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0x85, 0x80, //   Report ID (-128)
    0x09, 0x05, //   Usage (0x05)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0x85, 0x82, //   Report ID (-126)
    0x09, 0x06, //   Usage (0x06)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0xC0, // End Collection

    // 203 bytes
};

const uint8_t swpro_hid_report_descriptor_bt[] = {
    0x05, 0x01,                   // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                   // Usage (Game Pad)
    0xA1, 0x01,                   // Collection (Application)
    0x06, 0x01, 0xFF,             //   Usage Page (Vendor Defined 0xFF01)
    0x85, 0x21,                   //   Report ID (33)
    0x09, 0x21,                   //   Usage (0x21)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x30,                   //   Report ID (48)
    0x09, 0x30,                   //   Usage (0x30)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x31,                   //   Report ID (49)
    0x09, 0x31,                   //   Usage (0x31)
    0x75, 0x08,                   //   Report Size (8)
    0x96, 0x69, 0x01,             //   Report Count (361)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x32,                   //   Report ID (50)
    0x09, 0x32,                   //   Usage (0x32)
    0x75, 0x08,                   //   Report Size (8)
    0x96, 0x69, 0x01,             //   Report Count (361)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x33,                   //   Report ID (51)
    0x09, 0x33,                   //   Usage (0x33)
    0x75, 0x08,                   //   Report Size (8)
    0x96, 0x69, 0x01,             //   Report Count (361)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x3F,                   //   Report ID (63)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x01,                   //   Usage Minimum (0x01)
    0x29, 0x10,                   //   Usage Maximum (0x10)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x10,                   //   Report Count (16)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,                   //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,                   //   Usage (Hat switch)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x07,                   //   Logical Maximum (7)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x42,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x05, 0x09,                   //   Usage Page (Button)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x01,                   //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,                   //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,                   //   Usage (X)
    0x09, 0x31,                   //   Usage (Y)
    0x09, 0x33,                   //   Usage (Rx)
    0x09, 0x34,                   //   Usage (Ry)
    0x16, 0x00, 0x00,             //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //   Logical Maximum (65534)
    0x75, 0x10,                   //   Report Size (16)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x01, 0xFF,             //   Usage Page (Vendor Defined 0xFF01)
    0x85, 0x01,                   //   Report ID (1)
    0x09, 0x01,                   //   Usage (0x01)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x10,                   //   Report ID (16)
    0x09, 0x10,                   //   Usage (0x10)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x11,                   //   Report ID (17)
    0x09, 0x11,                   //   Usage (0x11)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x12,                   //   Report ID (18)
    0x09, 0x12,                   //   Usage (0x12)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x30,                   //   Report Count (48)
    0x91, 0x02,                   //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,                         // End Collection

    // 170 bytes
};

#define SWPRO_CONFIG_DESCRIPTOR_LEN 64
const uint8_t _swpro_configuration_descriptor[SWPRO_CONFIG_DESCRIPTOR_LEN] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    HUSB_CONFIG_DESCRIPTOR(1, 2, 0, SWPRO_CONFIG_DESCRIPTOR_LEN, HUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface
    9,
    HUSB_DESC_INTERFACE,
    0x00,
    0x00,
    0x02,
    HUSB_CLASS_HID,
    0x00,
    0x00,
    0x00,
    // HID Descriptor
    9,
    HHID_DESC_TYPE_HID,
    HUSB_U16_TO_U8S_LE(0x0111),
    0,
    1,
    HHID_DESC_TYPE_REPORT,
    HUSB_U16_TO_U8S_LE(sizeof(_swpro_hid_report_descriptor_usb)),
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x81,
    HUSB_XFER_INTERRUPT,
    HUSB_U16_TO_U8S_LE(64),
    4,
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x01,
    HUSB_XFER_INTERRUPT,
    HUSB_U16_TO_U8S_LE(64),
    8,

    // Alternate Interface for WebUSB
    // Interface
    9,
    HUSB_DESC_INTERFACE,
    0x01,
    0x00,
    0x02,
    HUSB_CLASS_VENDOR_SPECIFIC,
    0x00,
    0x00,
    0x00,
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x82,
    HUSB_XFER_BULK,
    HUSB_U16_TO_U8S_LE(64),
    0,
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x02,
    HUSB_XFER_BULK,
    HUSB_U16_TO_U8S_LE(64),
    0,
};

#define SWITCH_HID_NAME "Pro Controller"
const uint16_t _switch_hid_pid = 0x057E;
const uint16_t _switch_hid_vid = 0x2009;

const core_hid_device_t _switch_hid_device_usb = {
    .config_descriptor      = _swpro_configuration_descriptor,
    .config_descriptor_len  = SWPRO_CONFIG_DESCRIPTOR_LEN,
    .hid_report_descriptor  = _swpro_hid_report_descriptor_usb,
    .hid_report_descriptor_len = sizeof(_swpro_hid_report_descriptor_usb),
    .device_descriptor      = &_swpro_device_descriptor,
    .name = SWITCH_HID_NAME,
    .pid = _switch_hid_pid,
    .vid = _switch_hid_vid,
};

const core_hid_device_t _switch_hid_device_bt = {
    .config_descriptor = _swpro_configuration_descriptor,
    .config_descriptor_len = SWPRO_CONFIG_DESCRIPTOR_LEN,
    .hid_report_descriptor = swpro_hid_report_descriptor_bt,
    .hid_report_descriptor_len = 170,
    .device_descriptor = &_swpro_device_descriptor,
    .name = SWITCH_HID_NAME,
    .pid = _switch_hid_pid,
    .vid = _switch_hid_vid,
};

void _core_switch_report_tunnel_cb(const uint8_t *data, uint16_t len)
{
    if(len<2) return;

    uint8_t report_id = data[0];

    switch(report_id)
    {
        // If we only have rumble, we process and move on
        case SW_OUT_ID_RUMBLE:
        switch_haptics_rumble_translate(&data[2]);
        break;

        case SW_OUT_ID_RUMBLE_CMD:
        // Process rumble immediately, save data to process command
        // response on next packet generation
        switch_haptics_rumble_translate(&data[2]);

        // Fall through to default to process the command later
        default:
        // Copy the full data buffer, preserving the report ID
        memcpy(_scmd, data, len);
        break;
    }
}

bool _core_switch_get_generated_report(core_report_s *out)
{
    out->reportformat=CORE_REPORTFORMAT_SWPRO;
    out->size=_switch_report_size; // Includes report ID

    // Flush output buffer
    memset(out->data, 0, CORE_REPORT_DATA_LEN);

    // Check if we have command data
    // that we must respond to
    if(_scmd[0])
    {
        swcmd_generate_reply(_scmd, &out->data[0], &out->data[1]);
        // Clear command data
        _scmd[0]=0;
        memset(_scmd, 0, NS_CMD_DATA_LEN);
    }
    // Just generate input data and send
    else 
    {
        // Set the input data
        core_switch_report_s data = {0};

        mapper_input_s input = mapper_get_input();

        // Apply remapped data to our output buffer
        bool dpad[4] = {input.presses[SWITCH_CODE_DOWN], input.presses[SWITCH_CODE_RIGHT],
                    input.presses[SWITCH_CODE_LEFT], input.presses[SWITCH_CODE_UP]};

        dpad_translate_input(dpad);

        data.d_down     = dpad[0];
        data.d_right    = dpad[1];
        data.d_left     = dpad[2];
        data.d_up       = dpad[3];

        data.b_y = input.presses[SWITCH_CODE_Y];
        data.b_x = input.presses[SWITCH_CODE_X];
        data.b_a = input.presses[SWITCH_CODE_A];
        data.b_b = input.presses[SWITCH_CODE_B];

        data.b_minus    = input.presses[SWITCH_CODE_MINUS];
        data.b_plus     = input.presses[SWITCH_CODE_PLUS];
        data.b_home     = input.presses[SWITCH_CODE_HOME];
        data.b_capture  = input.presses[SWITCH_CODE_CAPTURE];

        data.sb_left  = input.presses[SWITCH_CODE_LS];
        data.sb_right = input.presses[SWITCH_CODE_RS];

        data.t_r = input.presses[SWITCH_CODE_R];
        data.t_l = input.presses[SWITCH_CODE_L];  
        data.t_zl = input.presses[SWITCH_CODE_ZL];
        data.t_zr = input.presses[SWITCH_CODE_ZR];

        uint16_t lx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LX_LEFT], input.inputs[SWITCH_CODE_LX_RIGHT]);
        uint16_t ly = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LY_DOWN], input.inputs[SWITCH_CODE_LY_UP]);
        uint16_t rx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RX_LEFT], input.inputs[SWITCH_CODE_RX_RIGHT]);
        uint16_t ry = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RY_DOWN], input.inputs[SWITCH_CODE_RY_UP]);

        lx = (uint16_t) CORE_SWITCH_CLAMP(lx, 0, 4095); 
        ly = (uint16_t) CORE_SWITCH_CLAMP(ly, 0, 4095); 
        rx = (uint16_t) CORE_SWITCH_CLAMP(rx, 0, 4095); 
        ry = (uint16_t) CORE_SWITCH_CLAMP(ry, 0, 4095); 
        
        // Custom mapping of bits for output for joysticks/buttons
        out->data[2] =  data.right_buttons;
        out->data[3] =  data.shared_buttons;
        out->data[4] =  data.left_buttons;
        out->data[5] =  (lx & 0xFF);
        out->data[6] =  (lx & 0xF00) >> 8;
        out->data[6] |= (ly & 0xF) << 4;
        out->data[7] =  (ly & 0xFF0) >> 4;
        out->data[8] =  (rx & 0xFF);
        out->data[9] =  (rx & 0xF00) >> 8;
        out->data[9] |= (ry & 0xF) << 4;
        out->data[10] =  (ry & 0xFF0) >> 4;

        swcmd_generate_inputreport(&out->data[0], &out->data[1]);
    }

    return true;
}

/*------------------------------------------------*/

// Public Functions
bool core_switch_init(core_params_s *params)
{
    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_USB:
        _switch_report_size = 64;
        params->hid_device = &_switch_hid_device_usb;
        params->core_pollrate_us = 8000;
        break;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        _switch_report_size = 49;
        params->hid_device = &_switch_hid_device_bt;
        params->core_pollrate_us = 8000;
        break;

        //case GAMEPAD_TRANSPORT_WLAN:
        //_switch_report_size = 64;
        //params->core_pollrate_us = 2000;
        //break;

        // Unsupported transport methods
        default:
        return false;
    }

    params->core_report_format       = CORE_REPORTFORMAT_SWPRO;
    params->core_report_generator    = _core_switch_get_generated_report;
    params->core_report_tunnel       = _core_switch_report_tunnel_cb;

    return transport_init(params);
}