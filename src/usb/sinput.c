#include "usb/sinput.h"

#include <stdbool.h>
#include <string.h>
#include "tusb.h"
#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"
#include "input/imu.h"
#include "input/trigger.h"

#include "utilities/pcm.h"
#include "utilities/static_config.h"

const tusb_desc_device_t sinput_device_descriptor = {
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

const uint8_t sinput_hid_report_descriptor[] = {

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

#define CLAMP(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))

static const int step_table[89] = {
     7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

void _decode_adpcm_buffer(const uint8_t *adpcm_data, uint8_t adpcm_len, int16_t *output, uint8_t *samples_decoded) {
    if (adpcm_len < 2 || !output || !samples_decoded) return;

    // Extract predictor seed
    int predictor = (int16_t)((adpcm_data[1] << 8) | adpcm_data[0]);
    int index = 0;
    size_t out_idx = 0;

    for (size_t i = 2; i < adpcm_len; ++i) {
        uint8_t byte = adpcm_data[i];
        for (int n = 0; n < 2; ++n) {
            uint8_t nibble = (n == 0) ? (byte & 0x0F) : (byte >> 4);
            int step = step_table[index];

            int diff = step >> 3;
            if (nibble & 1) diff += step >> 2;
            if (nibble & 2) diff += step >> 1;
            if (nibble & 4) diff += step;
            if (nibble & 8) diff = -diff;

            predictor += diff;
            predictor = CLAMP(predictor, -32768, 32767);

            output[out_idx++] = predictor;

            index += index_table[nibble & 0x0F];
            index = CLAMP(index, 0, 88);
        }
    }

    *samples_decoded = (uint8_t)out_idx;
}

void sinput_hid_process_pcm(const uint8_t *data, uint16_t len)
{
    // data[0] is the report ID (0x02 for PCM data)
    // data[1] is the type of haptic data
    
    uint8_t adpcm_length = data[2]; // Length of ADPCM data
    // Extract ADPCM data (skip report ID byte)
    const uint8_t *adpcm_data = &data[3];
    
    // Decode ADPCM to PCM
    int16_t decoded_samples[120]; // Maximum possible decoded samples
    uint8_t samples_decoded = 0;
    
    _decode_adpcm_buffer(adpcm_data, adpcm_length, decoded_samples, &samples_decoded);
    
    if(samples_decoded > 0)
    {
        pcm_raw_queue_push(decoded_samples, samples_decoded);
    }
}

// Feature Report ID 0x03 - Get Feature Flags
uint16_t sinput_hid_get_featureflags(uint8_t *buffer, uint16_t reqlen)
{
    sinput_featureflags_u feature_flags = {0};

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

    buffer[2] = 0x00; // Gamepad Sub-type (leave as zero in most cases)
    buffer[3] = 0x00; // Reserved byte

    buffer[4] = 0x00; // Reserved API version
    buffer[5] = 0x00; // Reserved

    memcpy(&buffer[6], &accel_g_range, sizeof(accel_g_range)); // Accelerometer G range
    memcpy(&buffer[8], &gyro_dps_range, sizeof(gyro_dps_range)); // Gyroscope DPS range

    // Copy device name
    memcpy(&buffer[10], device_static.name, 16); // Device name (up to 32 bytes, but we use 16 for now)
    
    return reqlen;
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

    //data.trigger_gl = buttons.trigger_gl;
    //data.trigger_gr = buttons.trigger_gr;

    data.button_power = buttons.button_shipping;

    int32_t trigger = -32768;

    data.trigger_l = trigger + ((triggers.left_analog)   * 16);    // Scale to 16-bit
    data.trigger_r = trigger + ((triggers.right_analog)  * 16);    // Scale to 16-bit

    memcpy(report_data, &data, sizeof(sinput_input_s));

    // If the PCM buffer is more than half full, set the buffer status to 0x01, otherwise set to 0x00
    report_data[33] = (pcm_raw_queue_count() > (PCM_RAW_QUEUE_SIZE >> 1)) ? 0x01 : 0; 

    last_timestamp = timestamp;
    cb(0x01, report_data, sizeof(sinput_input_s));
}