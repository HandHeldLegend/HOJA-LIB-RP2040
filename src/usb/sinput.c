#include "usb/sinput.h"

#include <stdbool.h>
#include <string.h>
#include "tusb.h"
#include "hoja.h"
#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"
#include "input/imu.h"
#include "input/trigger.h"

#include "utilities/pcm.h"
#include "utilities/static_config.h"

#include "board_config.h"

const ext_tusb_desc_device_t sinput_device_descriptor = {
    .bLength = 18,
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0210, // Changed from 0x0200 to 2.1 for BOS & WebUSB
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = 0x2E8A, // Raspberry Pi
    
    #if defined(HOJA_USB_PID)
    .idProduct = HOJA_USB_PID, // board_config PID
    #else
    .idProduct = 0x10C6, // Hoja Gamepad
    #endif

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
    };

const uint8_t sinput_hid_report_descriptor[203] = {

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

    0xC0                           // End Collection 
};



const uint8_t sinput_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 350),

    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(sinput_hid_report_descriptor)),
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4
};

void _sinput_cmd_haptics(const uint8_t *data)
{
    haptic_processed_s val = {0};

    sinput_haptic_s haptic = {0};

    memcpy(&haptic, data, sizeof(sinput_haptic_s));

    switch (haptic.type)
    {
        default:
        break;

        case 1:
            float a1_base = (float) (haptic.type_1.left.amplitude_1 > haptic.type_1.right.amplitude_1 ? haptic.type_1.left.amplitude_1 : haptic.type_1.right.amplitude_1);
            float a2_base = (float) (haptic.type_1.left.amplitude_2 > haptic.type_1.right.amplitude_2 ? haptic.type_1.left.amplitude_2 : haptic.type_1.right.amplitude_2);
            
            float amp_1 = a1_base > 0 ? (float) a1_base / (float) UINT16_MAX : 0;
            float amp_2 = a2_base > 0 ? (float) a2_base / (float) UINT16_MAX : 0;
        
            val.hi_amplitude_fixed = pcm_amplitude_to_fixedpoint(amp_1);
            val.lo_amplitude_fixed = pcm_amplitude_to_fixedpoint(amp_2);
        
            val.hi_frequency_increment = pcm_frequency_to_fixedpoint_increment((float) haptic.type_1.left.frequency_1);
            val.lo_frequency_increment = pcm_frequency_to_fixedpoint_increment((float) haptic.type_1.left.frequency_2);
        
            pcm_amfm_push(&val);
        break;

        case 2:
            bool left_right = haptic.type_2.left.amplitude > haptic.type_2.right.amplitude ? false : true; 

            uint8_t amp = 0;
            bool brake = 0;

            if(left_right)
            {
                amp = haptic.type_2.right.amplitude;
                brake = haptic.type_2.right.brake;
            }
            else 
            {
                amp = haptic.type_2.left.amplitude;
                brake = haptic.type_2.left.brake;
            }

            pcm_erm_set(amp, brake);
        break;
    } 
}

// Get Feature Flags
void _sinput_cmd_get_featureflags(uint8_t *buffer)
{
    sinput_featureflags_u feature_flags = {0};

    uint16_t accel_g_range      = 8; // 8G 
    uint16_t gyro_dps_range     = 2000; // 2000 degrees per second

    feature_flags.value = 0x00; // Set default feature flags

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

    buffer[2] = 0x00; // Gamepad Sub-type (leave as zero in most cases)
    buffer[3] = 0x00; // Reserved byte

    buffer[4] = 0x00; // Reserved API version
    buffer[5] = 0x00; // Reserved

    memcpy(&buffer[6], &accel_g_range, sizeof(accel_g_range)); // Accelerometer G range
    memcpy(&buffer[8], &gyro_dps_range, sizeof(gyro_dps_range)); // Gyroscope DPS range
}

volatile uint8_t _sinput_current_command = 0;
uint8_t _sinput_current_command_reply[30] = {0};

void sinput_hid_handle_command(const uint8_t *data)
{
    switch(data[0])
    {
        default:
        break;

        case SINPUT_COMMAND_HAPTIC:
        _sinput_cmd_haptics(&(data[1]));
        break;

        case SINPUT_COMMAND_FEATUREFLAGS:
        memset(_sinput_current_command_reply, 0, sizeof(_sinput_current_command_reply));
        _sinput_cmd_get_featureflags(_sinput_current_command_reply);
        _sinput_current_command = data[0];
        break;

        case SINPUT_COMMAND_PLAYERLED:
        uint8_t player_num = data[1];
        player_num = (player_num > 8) ? 8 : player_num; // Cap at 8
        hoja_set_connected_status(player_num);
        break;
    }
}

void sinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb)
{
    static uint8_t report_data[64] = {0};
    static sinput_input_s data = {0};
    static button_data_s buttons = {0};
    static analog_data_s analog  = {0};
    static trigger_data_s triggers = {0};
    imu_data_s imu = {0};

    static uint64_t last_timestamp = 0;

    uint64_t delta_timestamp = timestamp - last_timestamp;

    // Update input data
    remap_get_processed_input(&buttons, &triggers);
    analog_access_safe(&analog,  ANALOG_ACCESS_DEADZONE_DATA);

    data.left_x = analog.lx * 16;
    data.left_y = analog.ly * -16;
    data.right_x = analog.rx * 16;
    data.right_y = analog.ry * -16;

    imu_access_safe(&imu);

    data.accel_x = imu.ax; 
    data.accel_y = imu.ay;
    data.accel_z = imu.az;

    data.gyro_x = imu.gx;
    data.gyro_y = imu.gy;
    data.gyro_z = imu.gz;

    data.gyro_elapsed_time = delta_timestamp & 0xFFFF; // Store elapsed time in microseconds

    // Buttons
    data.button_a = buttons.button_a;
    data.button_b = buttons.button_b;
    data.button_x = buttons.button_x;
    data.button_y = buttons.button_y;

    data.button_stick_left = buttons.button_stick_left;
    data.button_stick_right = buttons.button_stick_right;

    data.button_plus = buttons.button_plus;
    data.button_minus = buttons.button_minus;
    data.button_home = buttons.button_home;
    data.button_capture = buttons.button_capture;

    data.dpad_up = buttons.dpad_up;
    data.dpad_down = buttons.dpad_down;
    data.dpad_left = buttons.dpad_left;
    data.dpad_right = buttons.dpad_right;

    data.button_l = buttons.trigger_l;
    data.button_r = buttons.trigger_r;

    data.button_zl = buttons.trigger_zl;
    data.button_zr = buttons.trigger_zr;

    data.button_gl = buttons.trigger_gl;
    data.button_gr = buttons.trigger_gr;

    data.button_power = buttons.button_shipping;

    int32_t trigger = -32768;

    if(analog_static.axis_lt)
        data.trigger_l = trigger + ((triggers.left_analog)   * 16);    // Scale to 16-bit
    else 
        data.trigger_l = -32768;

    if(analog_static.axis_rt)
        data.trigger_r = trigger + ((triggers.right_analog)  * 16);    // Scale to 16-bit
    else 
        data.trigger_r = -32768;

    if(_sinput_current_command != 0)
    {
        data.command_id = _sinput_current_command;
        memcpy(data.reserved_bulk, _sinput_current_command_reply, sizeof(_sinput_current_command_reply));
        _sinput_current_command = 0;
    }
    else 
    {
        data.command_id = 0;
        memset(data.reserved_bulk, 0, sizeof(data.reserved_bulk));
    }

    memcpy(report_data, &data, sizeof(sinput_input_s));

    last_timestamp = timestamp;
    cb(0x01, report_data, sizeof(sinput_input_s));
}