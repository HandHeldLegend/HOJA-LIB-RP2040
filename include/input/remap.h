#ifndef INPUT_REMAP_H
#define INPUT_REMAP_H

#include "input_shared_types.h"
#include "settings_shared_types.h"
#include "input/remap.h"
#include "devices/devices.h"

#define MAPCODE_MAX 16
// Map code is used during remap
// operations and configuration
typedef enum
{
    MAPCODE_DUP     = 0,
    MAPCODE_DDOWN   = 1,
    MAPCODE_DLEFT   = 2,
    MAPCODE_DRIGHT  = 3,

    MAPCODE_B_A = 4,
    MAPCODE_B_B = 5,

    MAPCODE_B_X = 6,
    MAPCODE_CUP = 6,

    MAPCODE_B_Y    = 7,
    MAPCODE_CDOWN  = 7,

    MAPCODE_T_L    = 8,
    MAPCODE_CLEFT  = 8,

    MAPCODE_T_ZL    = 9,

    MAPCODE_T_R     = 10,
    MAPCODE_CRIGHT  = 10,

    MAPCODE_T_ZR    = 11,

    MAPCODE_B_PLUS      = 12,
    MAPCODE_B_MINUS     = 13,
    MAPCODE_B_STICKL    = 14,
    MAPCODE_B_STICKR    = 15,
} mapcode_t;

typedef struct
{
    union
    {
        struct
        {
            bool dpad_up       : 1;
            bool dpad_down     : 1;
            bool dpad_left     : 1;
            bool dpad_right    : 1;
            bool button_a      : 1;
            bool button_b      : 1;
            bool button_x      : 1;
            bool button_y      : 1;
            bool trigger_l     : 1;
            bool trigger_zl    : 1;
            bool trigger_r     : 1;
            bool trigger_zr    : 1;
            bool button_plus   : 1;
            bool button_minus  : 1;
            bool button_stick_left     : 1;
            bool button_stick_right    : 1; 
        };
        uint16_t val;
    };
} buttons_unset_s;

void remap_config_cmd(remap_cmd_t cmd, const uint8_t *data, setting_callback_t cb);
void remap_get_processed_input(button_data_s *buttons_out, trigger_data_s *triggers_out);
void remap_init();

#endif
