#ifndef N64_H
#define N64_H

#include "hoja_includes.h"
#include "interval.h"

typedef enum
{
    N64_CMD_PROBE   = 0x00,
    N64_CMD_POLL    = 0x01,
    N64_CMD_READMEM = 0x02,
    N64_CMD_WRITEMEM = 0x03,
    N64_CMD_RESET = 0xFF
} n64_cmd_t;

typedef struct
{
    union
    {
        struct
        {
            uint8_t dpad_right  : 1;
            uint8_t dpad_left   : 1;
            uint8_t dpad_down   : 1;
            uint8_t dpad_up     : 1;
            uint8_t button_start : 1;
            uint8_t button_z    : 1;
            uint8_t button_b : 1;
            uint8_t button_a : 1;
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t cpad_right  : 1;
            uint8_t cpad_left   : 1;
            uint8_t cpad_down   : 1;
            uint8_t cpad_up     : 1;
            uint8_t button_r    : 1;
            uint8_t button_l    : 1;
            uint8_t reserved    : 1;
            uint8_t reset       : 1;
        };
        uint8_t buttons_2;
    };

    int8_t stick_x;
    int8_t stick_y;
} __attribute__ ((packed)) n64_input_s;

void n64_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog);

#endif 