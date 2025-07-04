#ifndef USB_SINPUT_H
#define USB_SINPUT_H

#include <stdint.h>
#include "utilities/callback.h"
#include "hoja_shared_types.h"

#define REPORT_ID_SINPUT_INPUT  0x01 // Input Report ID, used for SINPUT input data
#define REPORT_ID_SINPUT_OUTPUT 0x02 // Output Haptic Report ID, used for haptics and commands


#define SINPUT_COMMAND_HAPTIC       0x01
#define SINPUT_COMMAND_FEATUREFLAGS 0x02
#define SINPUT_COMMAND_PLAYERLED    0x03

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

    uint8_t command_id; // Response Command Byte Indication (0 if no response data)
    uint8_t reserved_bulk[30];  // Reserved for command data
} sinput_input_s;
#pragma pack(pop)

#pragma pack(push, 1) // Ensure byte alignment
typedef struct 
{
    uint8_t type;

    union 
    {
        // Frequency Amplitude pairs
        struct 
        {
            struct
            {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } left;

            struct
            {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } right;
            
        } type_1;

        // Basic ERM simulation model
        struct 
        {
            struct 
            {
                uint8_t amplitude;
                bool    brake;
            } left;

            struct 
            {
                uint8_t amplitude;
                bool    brake;
            } right;
            
        } type_2; 
    };
} sinput_haptic_s;
#pragma pack(pop)

#pragma pack(push, 1) // Ensure byte alignment
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
#pragma pack(pop)

extern const ext_tusb_desc_device_t sinput_device_descriptor;
extern const uint8_t sinput_hid_report_descriptor[203];
extern const uint8_t sinput_configuration_descriptor[];

void sinput_hid_handle_command(const uint8_t *data);
void sinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb);
#endif