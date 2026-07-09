#ifndef CORES_SLIPPI_H
#define CORES_SLIPPI_H

#include "cores/cores.h"

// Input structure for Nintendo GameCube Adapter USB Data
#pragma pack(push, 1) // Ensure byte alignment
typedef struct
{
    union
    {
        struct
        {
            uint8_t button_a    : 1;
            uint8_t button_b    : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;
            uint8_t dpad_left   : 1; //Left
            uint8_t dpad_right  : 1; //Right
            uint8_t dpad_down   : 1; //Down
            uint8_t dpad_up     : 1; //Up
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t button_start: 1;
            uint8_t button_z    : 1;
            uint8_t button_r    : 1;
            uint8_t button_l    : 1;
            uint8_t blank1      : 4;
        }; 
        uint8_t buttons_2;
    };

  uint8_t stick_x;
  uint8_t stick_y;
  uint8_t cstick_x;
  uint8_t cstick_y;
  uint8_t trigger_l;
  uint8_t trigger_r;

} core_slippi_report_s;
#pragma pack(pop)

bool core_slippi_init(core_params_s *params);

#endif