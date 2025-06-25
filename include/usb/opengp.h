#ifndef USB_OPENGP_H
#define USB_OPENGP_H

#include <stdint.h>
#include "tusb.h"
#include "utilities/callback.h"

#define REPORT_ID_OPENGP            0x01 // Input Report ID, used for OpenGP input data
#define REPORT_ID_OPENGP_HAPTICS    0x02 // Output Haptic Report ID, used for haptics commands and data
#define REPORT_ID_OPENGP_FEATURE_FEATFLAGS 0x03 // Output Command Report ID, used for OpenGP commands
#define REPORT_ID_OPENGP_FEATURE_PLAYERLED 0x04 // Input Command Report ID, used for OpenGP command responses

#pragma pack(push, 1) // Ensure byte alignment

// Input report (Report ID: 1)
typedef struct
{
    uint8_t plug_status;        // Plug Status Format
    uint8_t charge_percent;     // 0-100
    uint8_t buttons[4];         // 32 buttons (4 bytes * 8 bits)
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
    uint8_t reserved[31];       // Reserved bytes (33-63)
} opengp_input_s;

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
} opengp_featureflags_u;

typedef struct 
{
    uint16_t frequency_1;
    uint16_t amplitude_1;
    uint16_t frequency_2;
    uint16_t amplitude_2;
} opengp_haptic_sample_s;

typedef struct {
    uint8_t type;
    uint8_t sample_count; 
    opengp_haptic_sample_s samples[4];
    uint8_t reserved[29];
} opengp_haptic_report_t;

#define HAPTIC_REPORT_SIZE sizeof(opengp_haptic_report_t)

typedef struct {
    uint8_t report_id;             // 0x03
    uint8_t vendor_data[63];       // Vendor-specific data
} output_report_03_t;

#pragma pack(pop)

extern const tusb_desc_device_t opengp_device_descriptor;
extern const uint8_t opengp_hid_report_descriptor[];
extern const uint8_t opengp_configuration_descriptor[];

uint16_t opengc_hid_get_featureflags(uint8_t *buffer, uint16_t reqlen);

void opengp_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb);
#endif