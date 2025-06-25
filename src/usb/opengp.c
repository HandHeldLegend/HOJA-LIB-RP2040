#include "usb/opengp.h"

#include <stdbool.h>
#include <string.h>
#include "tusb.h"
#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"

#include "utilities/static_config.h"

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
    0x15, 0x00, // Logical Minimum (0)

    // INPUT REPORT ID 0x01 - Standard Gamepad (64 bytes, 1ms interval)

    0x09, 0x05,                    // Usage (Gamepad)
    0xA1, 0x01,                    // Collection (Application)

    0x85, 0x01,                    //   Report ID (1)
    
    // Vendor bytes (bytes 1-2) - Skip for DirectInput
    0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined)
    0x09, 0x01,                    //   Usage (Vendor Usage 1)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xFF,                    //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x02,                    //   Report Count (2)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Buttons (bytes 3-6) - 32 buttons total
    0x05, 0x09,                    //   Usage Page (Button)
    0x19, 0x01,                    //   Usage Minimum (Button 1)
    0x29, 0x20,                    //   Usage Maximum (Button 32)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x20,                    //   Report Count (32)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Analog Sticks and Triggers
    0x05, 0x01,                    //   Usage Page (Generic Desktop)
    
    // Left Stick X (bytes 7-8)
    0x09, 0x30,                    //   Usage (X)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Left Stick Y (bytes 9-10)
    0x09, 0x31,                    //   Usage (Y)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Right Stick X (bytes 11-12)
    0x09, 0x32,                    //   Usage (Z)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Right Stick Y (bytes 13-14)
    0x09, 0x35,                    //   Usage (Rz)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Left Trigger (bytes 15-16)
    0x09, 0x33,                    //   Usage (Rx)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Right Trigger (bytes 17-18)
    0x09, 0x34,                    //   Usage (Ry)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Gyro Elapsed Time (bytes 19-20)
    0x09, 0x3D,                    //   Usage (Start)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65535)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Motion Sensors (bytes 21-32) - Accelerometer and Gyroscope
    0x05, 0x20,                    //   Usage Page (Sensor)
    0x09, 0x73,                    //   Usage (Motion: Accelerometer 3D)
    
    // Accel X, Y, Z (bytes 21-26)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x03,                    //   Report Count (3)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Gyro X, Y, Z (bytes 27-32)
    0x09, 0x76,                    //   Usage (Motion: Gyrometer 3D)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x03,                    //   Report Count (3)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Reserved bytes (33-63) - 31 bytes
    0x05, 0x01,                    //   Usage Page (Generic Desktop)
    0x09, 0x00,                    //   Usage (Reserved)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x1F,                    //   Report Count (31)
    0x81, 0x03,                    //   Input (Const,Var,Abs)
    // End of Input Report for Gamepad
    
    // VENDOR REPORTS - Use vendor-defined usage page
    0x06, 0x00, 0xFF,              // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,                    // Usage (Vendor Usage 1)
    
    // OUTPUT REPORT ID 0x02 - Vendor haptic data (64 bytes, 4ms interval)
    0x85, 0x02,                    //   Report ID (2)
    0x09, 0x03,                    //   Usage (Vendor Usage 3)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, 0x3F,                    //   Report Count (63) - 64 bytes minus report ID
    0x91, 0x02,                    //   Output (Data,Var,Abs)
    
    // FEATURE REPORT ID 0x03 - Vendor device information data (64 bytes)
    0x85, 0x03,                    //   Report ID (3)
    0x09, 0x05,                    //   Usage (Vendor Usage 5)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, 0x3F,                    //   Report Count (63) - 64 bytes minus report ID
    0xB1, 0x02,                    //   Feature (Data,Var,Abs)

    // FEATURE REPORT ID 0x04 - Vendor LED data (64 bytes)
    0x85, 0x04,                    //   Report ID (4)
    0x09, 0x05,                    //   Usage (Vendor Usage 5)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, 0x3F,                    //   Report Count (63) - 64 bytes minus report ID
    0xB1, 0x02,                    //   Feature (Data,Var,Abs)

    0xC0                           // End Collection 
};

const uint8_t opengp_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 350),

    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(opengp_hid_report_descriptor)),
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4
};

volatile bool    _opengp_pending_command = false; // Flag to indicate if a command is pending
uint8_t _opengp_command_future[63] = {0}; // Buffer for command future data

// Feature Report ID 0x03 - Get Feature Flags
uint16_t opengc_hid_get_featureflags(uint8_t *buffer, uint16_t reqlen)
{
    opengp_featureflags_u feature_flags = {0};

    uint16_t accel_g_range      = 8; // 8G 
    uint16_t gyro_dps_range     = 2000; // 2000 degrees per second

    feature_flags.value = 0x00000000; // Set default feature flags

    feature_flags.accelerometer_supported   = (imu_static.axis_accel_a) ? 1 : 0;
    feature_flags.gyroscope_supported       = (imu_static.axis_gyro_a) ? 1 : 0;

    feature_flags.left_analog_stick_supported       = (analog_static.axis_lx) ? 1 : 0;
    feature_flags.right_analog_stick_supported      = (analog_static.axis_rx) ? 1 : 0;
    feature_flags.left_analog_trigger_supported     = (analog_static.axis_lt) ? 1 : 0;
    feature_flags.right_analog_trigger_supported    = (analog_static.axis_rt) ? 1 : 0;

    feature_flags.haptics_supported     = (haptic_static.haptic_hd | haptic_static.haptic_sd) ? 1 : 0;
    feature_flags.player_leds_supported = (rgb_static.rgb_player_group > -1) ? 1 : 0;

    buffer[0] = feature_flags.value; // Feature flags value      
    buffer[1] = 0x00; // Reserved byte

    buffer[2] = 0x00; // Version API (unused for now)
    buffer[3] = 0x00; // Version API (unused for now)

    memcpy(&buffer[4], &accel_g_range, sizeof(accel_g_range)); // Accelerometer G range
    memcpy(&buffer[6], &gyro_dps_range, sizeof(gyro_dps_range)); // Gyroscope DPS range

    // Copy device name
    memcpy(&buffer[8], device_static.name, 16); // Device name (up to 32 bytes, but we use 16 for now)
    
    return reqlen;
}

void _opengp_hid_command_handler(hid_report_tunnel_cb cb)
{
    

    
}

void opengp_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb)
{
    static uint8_t report_data[64] = {0};
    static opengp_input_s data = {0};
    static button_data_s buttons = {0};
    static analog_data_s analog  = {0};
    static trigger_data_s triggers = {0};

    // Update input data
    remap_get_processed_input(&buttons, &triggers);
    analog_access_safe(&analog,  ANALOG_ACCESS_DEADZONE_DATA);

    data.left_x = analog.lx * 16;
    data.left_y = analog.ly * -16;
    data.right_x = analog.rx * 16;
    data.right_y = analog.ry * -16;

    memcpy(report_data, &data, sizeof(opengp_input_s));

    // typedef bool (*hid_report_tunnel_cb)(uint8_t report_id, const void *report, uint16_t len)
    cb(0x01, report_data, sizeof(opengp_input_s));
}