#ifndef USB_SINPUT_H
#define USB_SINPUT_H

#include <stdint.h>
#include "tusb.h"
#include "utilities/callback.h"

#define REPORT_ID_SINPUT 0x01                   // Input Report ID, used for SINPUT input data
#define REPORT_ID_SINPUT_HAPTICS 0x02           // Output Haptic Report ID, used for haptics commands and data
#define REPORT_ID_SINPUT_FEATURE_FEATFLAGS 0x03 // Output Command Report ID, used for SINPUT commands
#define REPORT_ID_SINPUT_FEATURE_PLAYERLED 0x04 // Input Command Report ID, used for SINPUT command responses

#pragma pack(push, 1) // Ensure byte alignment
// Input report (Report ID: 1)
typedef struct
{
    uint8_t plug_status;    // Plug Status Format
    uint8_t charge_percent; // 0-100

    union
    {
        struct
        {
            uint8_t button_b : 1;
            uint8_t button_a : 1;
            uint8_t button_y : 1;
            uint8_t button_x : 1;
            uint8_t dpad_up : 1;
            uint8_t dpad_down : 1;
            uint8_t dpad_left : 1;
            uint8_t dpad_right : 1;
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t button_stick_left : 1;
            uint8_t button_stick_right : 1;
            uint8_t button_l : 1;
            uint8_t button_r : 1;
            uint8_t button_zl : 1;
            uint8_t button_zr : 1;
            uint8_t button_gl : 1;
            uint8_t button_gr : 1;
        };
        uint8_t buttons_2;
    };

    union
    {
        struct
        {
            uint8_t button_plus : 1;
            uint8_t button_minus : 1;
            uint8_t button_home : 1;
            uint8_t button_capture : 1;
            uint8_t button_power : 1;
            uint8_t reserved_b3 : 3; // Reserved bits
        };
        uint8_t buttons_3;
    };

    uint8_t buttons_reserved;
    int16_t left_x;             // Left stick X
    int16_t left_y;             // Left stick Y
    int16_t right_x;            // Right stick X
    int16_t right_y;            // Right stick Y
    int16_t trigger_l;          // Left trigger
    int16_t trigger_r;          // Right trigger
    uint16_t gyro_elapsed_time; // Microseconds, 0 if unchanged
    int16_t accel_x;            // Accelerometer X
    int16_t accel_y;            // Accelerometer Y
    int16_t accel_z;            // Accelerometer Z
    int16_t gyro_x;             // Gyroscope X
    int16_t gyro_y;             // Gyroscope Y
    int16_t gyro_z;             // Gyroscope Z
    uint8_t pcm_buffer_status;  // PCM buffer status (0x00 = empty, 0x01 = half full, 0x02 = full)
    uint8_t reserved_bulk[30];  // Reserved bytes (33-63)
} sinput_input_s;
#pragma pack(pop)

typedef union
{
    struct
    {
        uint8_t haptics_supported : 1;
        uint8_t player_leds_supported : 1;
        uint8_t accelerometer_supported : 1;
        uint8_t gyroscope_supported : 1;
        uint8_t left_analog_stick_supported : 1;
        uint8_t right_analog_stick_supported : 1;
        uint8_t left_analog_trigger_supported : 1;
        uint8_t right_analog_trigger_supported : 1;
    };
    uint8_t value;
} sinput_featureflags_u;

typedef struct
{
    uint8_t report_id;       // 0x03
    uint8_t vendor_data[63]; // Vendor-specific data
} output_report_03_t;

#define HAPTIC_SAMPLES_SIZE sizeof(sinput_haptic_sample_s)
#define HAPTIC_REPORT_SIZE  sizeof(sinput_haptic_report_t)

extern const tusb_desc_device_t sinput_device_descriptor;
extern const uint8_t sinput_hid_report_descriptor[];
extern const uint8_t sinput_configuration_descriptor[];

void sinput_hid_process_pcm(const uint8_t *data, uint16_t len);

uint16_t sinput_hid_get_featureflags(uint8_t *buffer, uint16_t reqlen);

void sinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb);
#endif