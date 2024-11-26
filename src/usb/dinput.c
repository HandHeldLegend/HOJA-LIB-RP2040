#include "devices/dinput.h"

/** DINPUT HID MODE **/
// 1. Device Descriptor
// 2. HID Report Descriptor
// 3. Configuration Descriptor
// 4. TinyUSB Config
/**--------------------------**/

const tusb_desc_device_t di_device_descriptor = {
    .bLength = 18,
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = 0x20d6,
    .idProduct = 0xa714,

    .bcdDevice = 0x0200,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,
    .bNumConfigurations = 0x01
};

// Generic Gamepad HID descriptor
const uint8_t di_hid_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)

    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
        0xA1, 0x01,         // Collection (Application)
            0x85, 0x01,        //   Report ID (1)

            0x05, 0x09,        //   Usage Page (Button)
            0x15, 0x00,        //   Logical Minimum (0)
            0x25, 0x01,        //   Logical Maximum (1)
            0x35, 0x00,        //   Physical Minimum (0)
            0x45, 0x01,        //   Physical Maximum (1)
            0x75, 0x01,        //   Report Size (1)
            0x95, 0x0E,        //   Report Count (14)
            0x19, 0x01,        //   Usage Minimum (0x01)
            0x29, 0x0E,        //   Usage Maximum (0x0E)
            0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

            0x95, 0x02,        //   Report Count (2)
            0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)

            0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
            0x25, 0x07,        //   Logical Maximum (7)
            0x46, 0x3B, 0x01,  //   Physical Maximum (315)
            0x75, 0x04,        //   Report Size (4)
            0x95, 0x01,        //   Report Count (1)
            0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
            0x09, 0x39,        //   Usage (Hat switch)
            0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
            0x65, 0x00,        //   Unit (None)
            0x95, 0x01,        //   Report Count (1)
            0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)

            0x26, 0xFF, 0x00,  //   Logical Maximum (255)
            0x46, 0xFF, 0x00,  //   Physical Maximum (255)
            0x09, 0x30,        //   Usage (X)
            0x09, 0x31,        //   Usage (Y)
            0x09, 0x32,        //   Usage (Z)
            0x09, 0x35,        //   Usage (Rz)
            0x09, 0x36,        //   Usage (Slider)
            0x09, 0x37,        //   Usage (Dial)

            0x75, 0x08,        //   Report Size (8)
            0x95, 0x06,        //   Report Count (6)
            0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        0xc0,

        0xA1, 0x01,         // Collection (Application)
            0x06, 0x00, 0xFF,      //            USAGE_PAGE (Vendor Defined Page 1)
            0x09, 0x01,            //            USAGE (Vendor Usage 1)
            0x85, 0x02,            //           Report ID (2)
            0x15, 0x00,            //            LOGICAL_MINIMUM (0)
            0x26, 0xff, 0x00,       //            LOGICAL_MAXIMUM (255)
            0x75, 0x08,            //            REPORT_SIZE (8)
            0x95, 0x0A,            //            REPORT_COUNT (10)
            0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xc0,

        0xA1, 0x01,         // Collection (Application)
            0x06, 0x00, 0xFF,      //            USAGE_PAGE (Vendor Defined Page 1)
            0x09, 0x01,            //            USAGE (Vendor Usage 1)
            0x85, 0x02,            //           Report ID (2)
            0x15, 0x00,            //            LOGICAL_MINIMUM (0)
            0x26, 0xff, 0x00,       //            LOGICAL_MAXIMUM (255)
            0x75, 0x08,            //            REPORT_SIZE (8)
            0x95, 0x0A,            //            REPORT_COUNT (10)
            0x91, 0x02,            //            OUTPUT (Data,Var,Abs)
        0xc0,
    // 125 bytes
    0xC0
};

const uint8_t di_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),
    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0110), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(di_hid_report_descriptor)),
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x02, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
};

void dinput_hid_report(button_data_s *button_data, a_data_s *analog_data)
{

}
