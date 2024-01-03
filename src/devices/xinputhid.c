#include "xinputhid.h"

typedef struct
{
    uint16_t stick_left_x   : 16;
    uint16_t stick_left_y   : 16;
    uint16_t stick_right_x  : 16;
    uint16_t stick_right_y  : 16;

    uint16_t analog_trigger_l : 16;
    uint16_t analog_trigger_r : 16;

    uint8_t dpad_hat        : 4;
    uint8_t dpad_padding    : 4;

    union
    {
        struct
        {
            uint8_t button_a    : 1;    
            uint8_t button_b    : 1;    
            uint8_t padding_1   : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;
            uint8_t padding_2   : 1;
            uint8_t bumper_l    : 1;
            uint8_t bumper_r    : 1;
        } __attribute__ ((packed)); 
        uint8_t buttons_1 : 8;
    };

    union
    {
        struct
        {
            uint8_t padding_3       : 2;
            uint8_t button_back     : 1; 
            uint8_t button_menu     : 1; 
            uint8_t button_guide    : 1; 
            uint8_t button_stick_l  : 1; 
            uint8_t button_stick_r      : 1; 
            uint8_t padding_4           : 1;  
        } __attribute__ ((packed));
        uint8_t buttons_2 : 8;
    };

    uint8_t buttons_blank;

} __attribute__ ((packed)) xi_input_s;

/** XINPUT HID MODE **/
/**--------------------------**/
const tusb_desc_device_t xhid_device_descriptor = {
    .bLength = 18,
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = 0x045E,
    .idProduct = 0x0B13,

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,
    .bNumConfigurations = 0x01};

// XInput HID Descriptor
const uint8_t xhid_hid_report_descriptor[] = {
    0x05, 0x01,                   // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                   // Usage (Game Pad)
    0xA1, 0x01,                   // Collection (Application)
    0x85, 0x01,                   //   Report ID (1)

    0x09, 0x01,                   //   Usage (Pointer)
    0xA1, 0x00,                   //   Collection (Physical)
    0x09, 0x30,                   //     Usage (X)
    0x09, 0x31,                   //     Usage (Y)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65534)
    0x95, 0x02,                   //     Report Count (2)
    0x75, 0x10,                   //     Report Size (16)
    0x81, 0x02,                   //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                         //   End Collection
    
    0x09, 0x01,                   //   Usage (Pointer)
    0xA1, 0x00,                   //   Collection (Physical)
    0x09, 0x32,                   //     Usage (Z)
    0x09, 0x35,                   //     Usage (Rz)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65534)
    0x95, 0x02,                   //     Report Count (2)
    0x75, 0x10,                   //     Report Size (16)
    0x81, 0x02,                   //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                         //   End Collection

    0x05, 0x02,                   //   Usage Page (Sim Ctrls)
    0x09, 0xC5,                   //   Usage (Brake)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x26, 0xFF, 0x03,             //   Logical Maximum (1023)
    0x95, 0x01,                   //   Report Count (1)
    0x75, 0x0A,                   //   Report Size (10)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x06,                   //   Report Size (6)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x05, 0x02,                   //   Usage Page (Sim Ctrls)
    0x09, 0xC4,                   //   Usage (Accelerator)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x26, 0xFF, 0x03,             //   Logical Maximum (1023)
    0x95, 0x01,                   //   Report Count (1)
    0x75, 0x0A,                   //   Report Size (10)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x06,                   //   Report Size (6)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,                   //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,                   //   Usage (Hat switch)
    0x15, 0x01,                   //   Logical Minimum (1)
    0x25, 0x08,                   //   Logical Maximum (8)
    0x35, 0x00,                   //   Physical Minimum (0)
    0x46, 0x3B, 0x01,             //   Physical Maximum (315)
    0x66, 0x14, 0x00,             //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x42,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x35, 0x00,                   //   Physical Minimum (0)
    0x45, 0x00,                   //   Physical Maximum (0)
    0x65, 0x00,                   //   Unit (None)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x01,                   //   Usage Minimum (0x01)
    0x29, 0x0F,                   //   Usage Maximum (0x0F)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x0F,                   //   Report Count (15)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,                   //   Usage Page (Consumer)
    0x0A, 0x24, 0x02,             //   Usage (AC Back)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x95, 0x01,                   //   Report Count (1)
    0x75, 0x01,                   //   Report Size (1)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x75, 0x07,                   //   Report Size (7)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x05, 0x0C,                   //   Usage Page (Consumer)
    0x09, 0x01,                   //   Usage (Consumer Control)
    0x85, 0x02,                   //   Report ID (2)
    0xA1, 0x01,                   //   Collection (Application)
    0x05, 0x0C,                   //     Usage Page (Consumer)
    0x0A, 0x23, 0x02,             //     Usage (AC Home)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x25, 0x01,                   //     Logical Maximum (1)
    0x95, 0x01,                   //     Report Count (1)
    0x75, 0x01,                   //     Report Size (1)
    0x81, 0x02,                   //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x25, 0x00,                   //     Logical Maximum (0)
    0x75, 0x07,                   //     Report Size (7)
    0x95, 0x01,                   //     Report Count (1)
    0x81, 0x03,                   //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                         //   End Collection
    0x05, 0x0F,                   //   Usage Page (PID Page)
    0x09, 0x21,                   //   Usage (0x21)
    0x85, 0x03,                   //   Report ID (3)
    0xA1, 0x02,                   //   Collection (Logical)
    0x09, 0x97,                   //     Usage (0x97)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x25, 0x01,                   //     Logical Maximum (1)
    0x75, 0x04,                   //     Report Size (4)
    0x95, 0x01,                   //     Report Count (1)
    0x91, 0x02,                   //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x25, 0x00,                   //     Logical Maximum (0)
    0x75, 0x04,                   //     Report Size (4)
    0x95, 0x01,                   //     Report Count (1)
    0x91, 0x03,                   //     Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x70,                   //     Usage (0x70)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x25, 0x64,                   //     Logical Maximum (100)
    0x75, 0x08,                   //     Report Size (8)
    0x95, 0x04,                   //     Report Count (4)
    0x91, 0x02,                   //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x50,                   //     Usage (0x50)
    0x66, 0x01, 0x10,             //     Unit (System: SI Linear, Time: Seconds)
    0x55, 0x0E,                   //     Unit Exponent (-2)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x26, 0xFF, 0x00,             //     Logical Maximum (255)
    0x75, 0x08,                   //     Report Size (8)
    0x95, 0x01,                   //     Report Count (1)
    0x91, 0x02,                   //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0xA7,                   //     Usage (0xA7)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x26, 0xFF, 0x00,             //     Logical Maximum (255)
    0x75, 0x08,                   //     Report Size (8)
    0x95, 0x01,                   //     Report Count (1)
    0x91, 0x02,                   //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x65, 0x00,                   //     Unit (None)
    0x55, 0x00,                   //     Unit Exponent (0)
    0x09, 0x7C,                   //     Usage (0x7C)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x26, 0xFF, 0x00,             //     Logical Maximum (255)
    0x75, 0x08,                   //     Report Size (8)
    0x95, 0x01,                   //     Report Count (1)
    0x91, 0x02,                   //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,                         //   End Collection
    0x05, 0x06,                   //   Usage Page (Generic Dev Ctrls)
    0x09, 0x20,                   //   Usage (Battery Strength)
    0x85, 0x04,                   //   Report ID (4)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x26, 0xFF, 0x00,             //   Logical Maximum (255)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                         // End Collection

    // 334 bytes
};

const uint8_t xhid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(xhid_hid_report_descriptor)),
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4
};

void xhid_hid_report(button_data_s *button_data, a_data_s *analog_data)
{
    static xi_input_s data = {.dpad_hat = XI_HAT_CENTER};
    static uint8_t output_buffer[64] = {0};

    data.stick_left_x = (analog_data->lx<<4);
    data.stick_left_y = (analog_data->ly<<4);
    data.stick_right_x = (analog_data->rx<<4);
    data.stick_right_y = (analog_data->ry<<4);

    memcpy(output_buffer, &data, sizeof(xi_input_s));
    tud_hid_report(0x01, output_buffer, 16);
}
