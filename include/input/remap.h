#ifndef REMAP_H
#define REMAP_H

#include <stdint.h>
#include "input/input.h"

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
            mapcode_t dpad_up     : 4;
            mapcode_t dpad_down   : 4;
            mapcode_t dpad_left   : 4;
            mapcode_t dpad_right  : 4;
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

typedef enum
{
    GC_SP_MODE_NONE = 0, // No function. LT and RT are output full according to digital button.
    GC_SP_MODE_LT   = 1, // SP buttton inputs light trigger left
    GC_SP_MODE_RT   = 2, // SP buttton inputs light trigger right
    GC_SP_MODE_TRAINING = 3, // Training mode reset
    GC_SP_MODE_DUALZ = 4, // Dual Z Button
    GC_SP_MODE_ADC  = 5, // Controlled fully by analog, SP button is unused

    GC_SP_MODE_CMD_SETLIGHT = 0xFF, // Command to set light trigger
} gc_sp_mode_t;

typedef struct
{
    button_remap_s  remap;
    buttons_unset_s disabled;
} remap_profile_s;

void remap_send_data_webusb(input_mode_t mode);
void remap_reset_default(input_mode_t mode);
void remap_set_gc_sp(gc_sp_mode_t sp_mode);
void remap_init(input_mode_t mode, button_data_s *in, button_data_s *out);
void remap_listen_stop();
void remap_listen_enable(input_mode_t mode, mapcode_t mapcode);
void remap_buttons_task();

#endif
