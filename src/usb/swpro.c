#include "usb/swpro.h"

#include "switch/switch_commands.h"
#include "utilities/callback.h"

#include "tusb.h"
#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"

/** Switch PRO HID MODE **/
// 1. Device Descriptor
// 2. HID Report Descriptor
// 3. Configuration Descriptor
// 4. TinyUSB Config
/**--------------------------**/
const ext_tusb_desc_device_t swpro_device_descriptor = {
    .bLength = 18,
    .bDescriptorType = TUSB_DESC_DEVICE,
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

const uint8_t swpro_hid_report_descriptor_usb[203] = {
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
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
0x09, 0x05,        // Usage (Game Pad)
0xA1, 0x01,        // Collection (Application)
0x06, 0x01, 0xFF,  //   Usage Page (Vendor Defined 0xFF01)
0x85, 0x21,        //   Report ID (33)
0x09, 0x21,        //   Usage (0x21)
0x75, 0x08,        //   Report Size (8)
0x95, 0x30,        //   Report Count (48)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x85, 0x30,        //   Report ID (48)
0x09, 0x30,        //   Usage (0x30)
0x75, 0x08,        //   Report Size (8)
0x95, 0x30,        //   Report Count (48)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x85, 0x31,        //   Report ID (49)
0x09, 0x31,        //   Usage (0x31)
0x75, 0x08,        //   Report Size (8)
0x96, 0x69, 0x01,  //   Report Count (361)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x85, 0x32,        //   Report ID (50)
0x09, 0x32,        //   Usage (0x32)
0x75, 0x08,        //   Report Size (8)
0x96, 0x69, 0x01,  //   Report Count (361)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x85, 0x33,        //   Report ID (51)
0x09, 0x33,        //   Usage (0x33)
0x75, 0x08,        //   Report Size (8)
0x96, 0x69, 0x01,  //   Report Count (361)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x85, 0x3F,        //   Report ID (63)
0x05, 0x09,        //   Usage Page (Button)
0x19, 0x01,        //   Usage Minimum (0x01)
0x29, 0x10,        //   Usage Maximum (0x10)
0x15, 0x00,        //   Logical Minimum (0)
0x25, 0x01,        //   Logical Maximum (1)
0x75, 0x01,        //   Report Size (1)
0x95, 0x10,        //   Report Count (16)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
0x09, 0x39,        //   Usage (Hat switch)
0x15, 0x00,        //   Logical Minimum (0)
0x25, 0x07,        //   Logical Maximum (7)
0x75, 0x04,        //   Report Size (4)
0x95, 0x01,        //   Report Count (1)
0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
0x05, 0x09,        //   Usage Page (Button)
0x75, 0x04,        //   Report Size (4)
0x95, 0x01,        //   Report Count (1)
0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
0x09, 0x30,        //   Usage (X)
0x09, 0x31,        //   Usage (Y)
0x09, 0x33,        //   Usage (Rx)
0x09, 0x34,        //   Usage (Ry)
0x16, 0x00, 0x00,  //   Logical Minimum (0)
0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65534)
0x75, 0x10,        //   Report Size (16)
0x95, 0x04,        //   Report Count (4)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x06, 0x01, 0xFF,  //   Usage Page (Vendor Defined 0xFF01)
0x85, 0x01,        //   Report ID (1)
0x09, 0x01,        //   Usage (0x01)
0x75, 0x08,        //   Report Size (8)
0x95, 0x30,        //   Report Count (48)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x10,        //   Report ID (16)
0x09, 0x10,        //   Usage (0x10)
0x75, 0x08,        //   Report Size (8)
0x95, 0x30,        //   Report Count (48)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x11,        //   Report ID (17)
0x09, 0x11,        //   Usage (0x11)
0x75, 0x08,        //   Report Size (8)
0x95, 0x30,        //   Report Count (48)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0x85, 0x12,        //   Report ID (18)
0x09, 0x12,        //   Usage (0x12)
0x75, 0x08,        //   Report Size (8)
0x95, 0x30,        //   Report Count (48)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              // End Collection

// 170 bytes

};

const uint8_t swpro_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, 64, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(swpro_hid_report_descriptor_usb)),
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 8,

    // Alternate Interface for WebUSB
    // Interface
    9, TUSB_DESC_INTERFACE, 0x01, 0x00, 0x02, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, 0x00,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x82, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
};

/**--------------------------**/
/**--------------------------**/

bool mac_sent = false;
bool blank_sent = false;

uint32_t _timeout = 0;

void swpro_hid_report(uint32_t timestamp, hid_report_tunnel_cb cb)
{
    static sw_input_s data = {0};

    static button_data_s buttons = {0};
    static analog_data_s analog  = {0};
    static trigger_data_s triggers = {0};

    // Update input data
    remap_get_processed_input(&buttons, &triggers);
    analog_access_safe(&analog,  ANALOG_ACCESS_DEADZONE_DATA);

    data.d_down     = buttons.dpad_down;
    data.d_right    = buttons.dpad_right;
    data.d_left     = buttons.dpad_left;
    data.d_up       = buttons.dpad_up;

    data.b_y = buttons.button_y;
    data.b_x = buttons.button_x;
    data.b_a = buttons.button_a;
    data.b_b = buttons.button_b;

    data.b_minus    = buttons.button_minus;
    data.b_plus     = buttons.button_plus;
    data.b_home     = buttons.button_home;
    data.b_capture  = buttons.button_capture;

    data.sb_right   = buttons.button_stick_right;
    data.sb_left    = buttons.button_stick_left;

    data.t_r = buttons.trigger_r;
    data.t_l = buttons.trigger_l;
    
    data.t_zl = buttons.trigger_zl;
    data.t_zr = buttons.trigger_zr;

    data.ls_x = (uint16_t) (analog.lx+2048); 
    data.ls_y = (uint16_t) (analog.ly+2048);
    data.rs_x = (uint16_t) (analog.rx+2048);
    data.rs_y = (uint16_t) (analog.ry+2048);

    data.ls_x = (data.ls_x > 4095) ? 4095 : data.ls_x;
    data.ls_y = (data.ls_y > 4095) ? 4095 : data.ls_y;
    data.rs_x = (data.rs_x > 4095) ? 4095 : data.rs_x;
    data.rs_y = (data.rs_y > 4095) ? 4095 : data.rs_y;

    switch_commands_process(&data, cb);
}