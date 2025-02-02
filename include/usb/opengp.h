#ifndef USB_OPENGP_H
#define USB_OPENGP_H

#include <stdint.h>
#include "tusb.h"

#define REPORT_ID_OPENGP 0x01


#pragma pack(push, 1)  // Ensure byte alignment

// Input report (Report ID: 1)
typedef struct {
    // Joysticks
    uint8_t left_x;        // Left joystick X
    uint8_t left_y;        // Left joystick Y
    uint8_t right_x;       // Right joystick X (Rx)
    uint8_t right_y;       // Right joystick Y (Ry)
    
    // Triggers
    uint8_t trigger_l;     // Left trigger (Z)
    uint8_t trigger_r;     // Right trigger (Rz)
    
    // Buttons (16 buttons total)
    struct {
        uint8_t button_a : 1;
        uint8_t button_x : 1;
        uint8_t button_r : 1;
        uint8_t button_b : 1;
        uint8_t button_y : 1;
        uint8_t button_l : 1;
        uint8_t button_zl : 1;
        uint8_t button_zr : 1;
        uint8_t button_minus : 1;    // Select
        uint8_t button_plus : 1;     // Start
        uint8_t button_home : 1;
        uint8_t button_l3 : 1;       // Left stick press
        uint8_t button_r3 : 1;       // Right stick press
        uint8_t button_capture : 1;
        uint8_t reserved1 : 1;       // Padding to match 16 buttons
        uint8_t reserved2 : 1;
    };
    
    // D-Pad (as hat switch) and padding
    struct {
        uint8_t dpad : 4;    // 0-7 for 8 directions, 8 for center
        uint8_t : 4;         // Padding bits
    };
} opengp_input_s;

// Output report (Report ID: 2)
typedef struct {
    uint8_t report_id;     // Always 2
    uint8_t rumble;        // Rumble intensity
    
    // Player LEDs
    struct {
        uint8_t player1 : 1;
        uint8_t player2 : 1;
        uint8_t player3 : 1;
        uint8_t player4 : 1;
        uint8_t player5 : 1;
        uint8_t player6 : 1;
        uint8_t player7 : 1;
        uint8_t player8 : 1;
    } leds;
} opengp_output_s;
#pragma pack(pop)

// D-Pad direction values
enum dpad_direction {
    DPAD_N   = 0,
    DPAD_NE  = 1,
    DPAD_E   = 2,
    DPAD_SE  = 3,
    DPAD_S   = 4,
    DPAD_SW  = 5,
    DPAD_W   = 6,
    DPAD_NW  = 7,
    DPAD_CENTER = 8
};

extern const tusb_desc_device_t opengp_device_descriptor;
extern const uint8_t opengp_hid_report_descriptor[];
extern const uint8_t opengp_configuration_descriptor[];

void opengp_hid_report(uint32_t timestamp);
#endif