#ifndef INPUT_REMAP_H
#define INPUT_REMAP_H

#include "input_shared_types.h"
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

// Remapping struct used to determine
// remapping parameters
typedef struct
{
    union
    {
        struct
        {
            mapcode_t dpad_up     : 4; // Default 0
            mapcode_t dpad_down   : 4; // Default 1
            mapcode_t dpad_left   : 4; // Default 2
            mapcode_t dpad_right  : 4; // Default 3...
            mapcode_t button_a      : 4;
            mapcode_t button_b      : 4;
            mapcode_t button_x      : 4;
            mapcode_t button_y      : 4;
            mapcode_t trigger_l       : 4;
            mapcode_t trigger_zl      : 4;
            mapcode_t trigger_r       : 4;
            mapcode_t trigger_zr      : 4;
            mapcode_t button_plus   : 4;
            mapcode_t button_minus  : 4;
            mapcode_t button_stick_left     : 4;
            mapcode_t button_stick_right    : 4;
        };
        uint64_t val;
    };
} button_remap_s;

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

typedef struct
{
    button_remap_s  remap;
    buttons_unset_s disabled;
} remap_profile_s;

void remap_process(button_data_s *in, button_data_s *out);

#endif
