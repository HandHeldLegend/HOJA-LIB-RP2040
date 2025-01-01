#ifndef WIRED_GAMECUBE_H
#define WIRED_GAMECUBE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum{
    GCUBE_CMD_PROBE     = 0x00,
    GCUBE_CMD_POLL      = 0x40,
    GCUBE_CMD_ORIGIN    = 0x41,
    GCUBE_CMD_ORIGINEXT = 0x42,
    GCUBE_CMD_SWISS     = 0x1D,
} gc_cmd_t;

// Additional modes, ideas taken from
// https://github.com/PhobGCC/PhobGCC-SW/commit/dfeab45d68075e6df8cd3b303304b5169ffa8bea
// Thanks to https://github.com/mizuyoukanao
typedef struct
{
    union
    {
        struct
        {
            uint8_t a : 1; uint8_t b : 1; uint8_t x:1; uint8_t y : 1; uint8_t start : 1; uint8_t blank_1 : 3;
        };
        uint8_t buttons_1;  
    };

    union
    {
        struct
        {
            uint8_t dpad_left : 1; uint8_t dpad_right : 1; uint8_t dpad_down : 1; uint8_t dpad_up : 1; uint8_t z : 1; uint8_t r : 1; uint8_t l : 1; uint8_t blank_2 : 1;
        };
        uint8_t buttons_2;
    };

    // Analog mode union
    union
    {
        struct
        {
            uint8_t stick_left_x;
            uint8_t stick_left_y;
            uint8_t stick_right_x;
            uint8_t stick_right_y;
            uint8_t analog_trigger_l;
            uint8_t analog_trigger_r;
        }; // Default mode 3

        struct
        {
            uint8_t stick_left_x;
            uint8_t stick_left_y;
            uint8_t stick_right_x;
            uint8_t stick_right_y;
            uint8_t analog_trigger_r : 4;
            uint8_t analog_trigger_l : 4;
            uint8_t analog_b : 4;
            uint8_t analog_a : 4;
        } mode0;

        struct
        {
            uint8_t stick_left_x;
            uint8_t stick_left_y;
            uint8_t stick_right_y : 4;
            uint8_t stick_right_x : 4;
            uint8_t analog_trigger_l;
            uint8_t analog_trigger_r;
            uint8_t analog_b : 4;
            uint8_t analog_a : 4;
        } mode1;

        struct
        {
            uint8_t stick_left_x;
            uint8_t stick_left_y;
            uint8_t stick_right_y : 4;
            uint8_t stick_right_x : 4;
            uint8_t analog_trigger_r : 4;
            uint8_t analog_trigger_l : 4;
            uint8_t analog_a;
            uint8_t analog_b;
        } mode2;

        struct
        {
            uint8_t stick_left_x;
            uint8_t stick_left_y;
            uint8_t stick_right_x;
            uint8_t stick_right_y;
            uint8_t analog_a;
            uint8_t analog_b;
        } mode4;
    };
} gamecube_input_s;

void gamecube_wired_task(uint32_t timestamp);
bool gamecube_wired_start();

#endif