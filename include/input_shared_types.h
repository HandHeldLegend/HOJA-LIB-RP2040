#ifndef INPUT_SHARED_TYPES_H
#define INPUT_SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/** @brief This is a struct for containing all of the
 * button input data as bits. This saves space
 * and allows for easier handoff to the various
 * controller cores in the future.
**/
typedef struct
{
    union
    {
        struct
        {
            // D-Pad
            uint8_t dpad_up     : 1;
            uint8_t dpad_down   : 1;
            uint8_t dpad_left   : 1;
            uint8_t dpad_right  : 1;
            // Buttons
            uint8_t button_a    : 1;
            uint8_t button_b    : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;

            // Triggers
            uint8_t trigger_l   : 1;
            uint8_t trigger_zl  : 1;
            uint8_t trigger_r   : 1;
            uint8_t trigger_zr  : 1;

            // Special Functions
            uint8_t button_plus     : 1;
            uint8_t button_minus    : 1;

            // Stick clicks
            uint8_t button_stick_left   : 1;
            uint8_t button_stick_right  : 1;
        };
        uint16_t buttons_all;
    };

    union
    {
        struct
        {
            // Menu buttons (Not remappable by API)
            uint8_t button_capture  : 1;
            uint8_t button_home     : 1;
            uint8_t button_safemode : 1;
            uint8_t button_shipping : 1;
            uint8_t button_sync     : 1;
            uint8_t button_unbind   : 1;
            uint8_t padding         : 2;
        };
        uint8_t buttons_system;
    };

    int zl_analog;
    int zr_analog;
} button_data_s;

// Analog input data structure
typedef struct
{
    int lx;
    int ly;
    int rx;
    int ry;
} analog_data_s;

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

#endif