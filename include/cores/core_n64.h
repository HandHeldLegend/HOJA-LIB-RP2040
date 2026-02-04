#ifndef CORES_N64_H
#define CORES_N64_H

#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

typedef struct
{
    union
    {
        struct
        {
            uint8_t dpad_right : 1;
            uint8_t dpad_left : 1;
            uint8_t dpad_down : 1;
            uint8_t dpad_up : 1;
            uint8_t button_start : 1;
            uint8_t button_z : 1;
            uint8_t button_b : 1;
            uint8_t button_a : 1;
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t cpad_right : 1;
            uint8_t cpad_left : 1;
            uint8_t cpad_down : 1;
            uint8_t cpad_up : 1;
            uint8_t button_r : 1;
            uint8_t button_l : 1;
            uint8_t reserved : 1;
            uint8_t reset : 1;
        };
        uint8_t buttons_2;
    };

    int8_t stick_x;
    int8_t stick_y;
} core_n64_report_s;
#define CORE_N64_REPORT_SIZE sizeof(core_n64_report_s)

bool core_n64_init(core_params_s *params);

#endif