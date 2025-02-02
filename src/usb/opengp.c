#include "usb/opengp.h"

#include <stdbool.h>
#include <string.h>
#include "tusb.h"
#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"

const tusb_desc_device_t opengp_device_descriptor = {
    .bLength = 18,
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0210, // Changed from 0x0200 to 2.1 for BOS & WebUSB
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = 0x2E8A, // Raspberry Pi
    .idProduct = 0x10C6, // Hoja Gamepad

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
    };

const uint8_t opengp_hid_report_descriptor[] = {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05, // Usage (Game Pad)
    0xA1, 0x01, // Collection (Application)

    // Report ID for input
    0x85, 0x01, // Report ID (1)

    // Left Joystick X and Y
    0x09, 0x01,       // Usage (Pointer)
    0xA1, 0x00,       // Collection (Physical)
    0x09, 0x30,       //   Usage (X)
    0x09, 0x31,       //   Usage (Y)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x02,       //   Report Count (2)
    0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,             // End Collection

    // Right Joystick X and Y
    0x09, 0x01,       // Usage (Pointer)
    0xA1, 0x00,       // Collection (Physical)
    0x09, 0x33,       //   Usage (Rx)
    0x09, 0x34,       //   Usage (Ry)
    0x15, 0x00,       //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x02,       //   Report Count (2)
    0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,             // End Collection

    // Left and Right Triggers
    0x09, 0x32,       // Usage (Z) - Left Trigger
    0x09, 0x35,       // Usage (Rz) - Right Trigger
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x00, // Logical Maximum (255)
    0x75, 0x08,       // Report Size (8)
    0x95, 0x02,       // Report Count (2)
    0x81, 0x02,       // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    // Buttons (A, X, R, B, Y, L, ZL, ZR, 0, 0, Select, Start, Home, L3, R3, Capture)
    0x05, 0x09, // Usage Page (Button)
    0x19, 0x01, // Usage Minimum (Button 1)
    0x29, 0x10, // Usage Maximum (Button 16)
    0x15, 0x00, // Logical Minimum (0)
    0x25, 0x01, // Logical Maximum (1)
    0x75, 0x01, // Report Size (1)
    0x95, 0x10, // Report Count (16)
    0x81, 0x02, // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    // D-Pad (as a Hat switch)
    0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,       // Usage (Hat switch)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x07,       // Logical Maximum (7)
    0x35, 0x00,       // Physical Minimum (0)
    0x46, 0x3B, 0x01, // Physical Maximum (315)
    0x65, 0x14,       // Unit (Degrees)
    0x75, 0x04,       // Report Size (4)
    0x95, 0x01,       // Report Count (1)
    0x81, 0x42,       // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)

    // Padding (to align the D-Pad to a full byte)
    0x75, 0x04, // Report Size (4)
    0x95, 0x01, // Report Count (1)
    0x81, 0x03, // Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    // Output report (for rumble and player number)
    0x85, 0x02, // Report ID (2)
              
    // Rumble
    0x05, 0x0F,       // Usage Page (PID Page)
    0x09, 0x97,       // Usage (DC Enable Actuators)
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x00, // Logical Maximum (255)
    0x75, 0x08,       // Report Size (8)
    0x95, 0x01,       // Report Count (1)
    0x91, 0x02,       // Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
              
    // Player LEDs
    0x05, 0x08, // Usage Page (LEDs)
    0x19, 0x61, // Usage Minimum (Player 1)
    0x29, 0x68, // Usage Maximum (Player 8)
    0x15, 0x00, // Logical Minimum (0)
    0x25, 0x01, // Logical Maximum (1)
    0x75, 0x01, // Report Size (1)
    0x95, 0x08, // Report Count (8)
    0x91, 0x02, // Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

    0xC0 // End Collection
};

const uint8_t opengp_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(opengp_hid_report_descriptor)),
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1
};

uint8_t _dpad_direction(uint8_t up, uint8_t down, uint8_t left, uint8_t right)
{
    static int8_t ud, lr;

    if(up && !down) ud = 1;
    else if(!up && down) ud = -1;
    else if(!up && !down) ud = 0;

    if(left && !right) lr = -1;
    else if(!left && right) lr = 1;
    else if(!left && !right) lr = 0;

    switch(ud)
    {
        default:
        case 0:
            if(!lr) return DPAD_CENTER;
            return (lr>0) ? DPAD_E : DPAD_W;
        case 1:
            if(!lr) return DPAD_N;
            return (lr>0) ? DPAD_NE : DPAD_NW;
        case -1:
            if(!lr) return DPAD_S;
            return (lr>0) ? DPAD_SE : DPAD_SW;
    }
}

void opengp_hid_report(uint32_t timestamp)
{
    static uint8_t report_data[64] = {0};
    static opengp_input_s data = {0};
    static button_data_s buttons = {0};
    static analog_data_s analog  = {0};
    static trigger_data_s triggers = {0};

    // Update input data
    remap_get_processed_input(&buttons, &triggers);
    analog_access_safe(&analog,  ANALOG_ACCESS_DEADZONE_DATA);

    data.dpad = _dpad_direction(buttons.dpad_up, buttons.dpad_down, buttons.dpad_left, buttons.dpad_right);

    data.button_y = buttons.button_y;
    data.button_x = buttons.button_x;
    data.button_a = buttons.button_a;
    data.button_b = buttons.button_b;

    data.button_minus    = buttons.button_minus;
    data.button_plus     = buttons.button_plus;
    data.button_home     = buttons.button_home;
    data.button_capture  = buttons.button_capture;

    data.button_r3   = buttons.button_stick_right;
    data.button_l3    = buttons.button_stick_left;

    data.button_r = buttons.trigger_r;
    data.button_l = buttons.trigger_l;
    
    data.button_zl = buttons.trigger_zl;
    data.button_zr = buttons.trigger_zr;

    // ANALOG STICKS
    const int32_t center_value = 128;
    const float   target_max = 129 / 2048.0f;
    const int32_t fixed_multiplier = (int32_t) (target_max * (1<<16));

    bool lx_sign = analog.lx < 0;
    bool ly_sign = analog.ly < 0;
    bool rx_sign = analog.rx < 0;
    bool ry_sign = analog.ry < 0;

    uint32_t lx_abs = lx_sign ? -analog.lx : analog.lx;
    uint32_t ly_abs = ly_sign ? -analog.ly : analog.ly;
    uint32_t rx_abs = rx_sign ? -analog.rx : analog.rx;
    uint32_t ry_abs = ry_sign ? -analog.ry : analog.ry;

    // Analog stick data conversion
    int32_t lx = ((lx_abs * fixed_multiplier) >> 16) * (lx_sign ? -1 : 1);
    int32_t ly = ((ly_abs * fixed_multiplier) >> 16) * (ly_sign ? -1 : 1);
    int32_t rx = ((rx_abs * fixed_multiplier) >> 16) * (rx_sign ? -1 : 1);
    int32_t ry = ((ry_abs * fixed_multiplier) >> 16) * (ry_sign ? -1 : 1);

    #define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
    uint8_t lx8 = CLAMP_0_255(lx + center_value);
    uint8_t ly8 = CLAMP_0_255(ly + center_value);
    uint8_t rx8 = CLAMP_0_255(rx + center_value);
    uint8_t ry8 = CLAMP_0_255(ry + center_value);

    data.left_x = lx8;
    data.left_y = 0xFF-ly8;
    data.right_x = rx8;
    data.right_y = ry8;

    memcpy(report_data, &data, sizeof(opengp_input_s));

    tud_hid_report(0x01, report_data, sizeof(opengp_input_s));
}