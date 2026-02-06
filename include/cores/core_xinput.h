#ifndef CORES_XINPUT_H
#define CORES_XINPUT_H

#include "cores/cores.h"

#define CORE_XINPUT_REPORT_LEN 20
#define CORE_XINPUT_REPORT_ID  0x00

typedef struct
{
    uint8_t report_id;
	uint8_t report_size;
	union
    {
        struct
        {
            uint8_t dpad_up     : 1;
            uint8_t dpad_down   : 1;
            uint8_t dpad_left      : 1;
            uint8_t dpad_right      : 1;
            uint8_t button_menu     : 1;
            uint8_t button_back     : 1;
            uint8_t button_stick_l  : 1;
            uint8_t button_stick_r  : 1;
        };
        uint8_t buttons_1 : 8;
    };
	union
    {
        struct
        {
            uint8_t bumper_l    : 1;
            uint8_t bumper_r    : 1;
            uint8_t button_guide: 1;
            uint8_t blank_1     : 1;
            uint8_t button_a    : 1;
            uint8_t button_b    : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;
        };
        uint8_t buttons_2 : 8;
    };
	uint8_t analog_trigger_l;
	uint8_t analog_trigger_r;
	short stick_left_x;
	short stick_left_y;
	short stick_right_x;
	short stick_right_y;
	uint8_t reserved_1[6];
} core_xinput_report_s;

#define CORE_XINPUT_REPORT_SIZE 64 // Send 64 bytes...

bool core_xinput_init(core_params_s *params);

#endif