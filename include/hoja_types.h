#ifndef HOJA_TYPES_H
#define HOJA_TYPES_H

#include <inttypes.h>


/**
 * 
 * uint8_t array[0]:  | analog_stick_left | analog_stick_right | analog_trigger_left | analog_trigger_right | gyroscope | bluetooth | rgb | rumble |
                   |        1 bit      |        1 bit       |         1 bit       |          1 bit       |   1 bit   |   1 bit   | 1 bit | 1 bit |

 * uint8_t array[1]:  | nintendo_serial | nintendo_joybus | padding  | padding  | padding  | padding  | padding  | padding  |
                   |      1 bit         |      1 bit      |   1 bit  |   1 bit  |   1 bit  |   1 bit  |   1 bit  |   1 bit  |
*/

typedef struct
{
    uint8_t analog_stick_left      : 1;
    uint8_t analog_stick_right     : 1;
    uint8_t analog_trigger_left    : 1;
    uint8_t analog_trigger_right   : 1;
    uint8_t gyroscope : 1;
    uint8_t bluetooth : 1;
    uint8_t rgb : 1;
    uint8_t rumble : 1;
    uint8_t nintendo_serial : 1;
    uint8_t nintendo_joybus : 1;
    uint8_t padding : 6;
} hoja_capabilities_t;

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
    GC_SP_MODE_ADC  = 4, // Controlled fully by analog, SP button is unused

    GC_SP_MODE_CMD_SETLIGHT = 0xFF, // Command to set light trigger
} gc_sp_mode_t;

typedef struct
{
    union
    {
        struct
        {
            uint8_t padding : 8;
            uint8_t b : 8;
            uint8_t r : 8;
            uint8_t g : 8;
        };
        uint32_t color;
    };
} rgb_s;

typedef struct {
    rgb_s rs;
    rgb_s ls;
    rgb_s dpad;
    rgb_s minus;
    rgb_s capture;
    rgb_s home;
    rgb_s plus;
    rgb_s y;
    rgb_s x;
    rgb_s a;
    rgb_s b;
} rgb_preset_t;

typedef enum
{
    INPUT_MODE_SWPRO    = 0,
    INPUT_MODE_XINPUT   = 1,
    INPUT_MODE_GAMECUBE = 2,
    INPUT_MODE_N64      = 3,
    INPUT_MODE_SNES     = 4,
    INPUT_MODE_GCUSB    = 5,
} input_mode_t;

typedef enum
{
    USBRATE_8 = 7600,
    USBRATE_1 = 100,
} usb_rate_t;

typedef enum
{
    CALIBRATE_START,
    CALIBRATE_CANCEL,
    CALIBRATE_SAVE,
} calibrate_set_t;

typedef enum
{
  RUMBLE_OFF,
  RUMBLE_BRAKE,
  RUMBLE_ON,
} rumble_t;

typedef enum
{
  NS_HAT_TOP          = 0x00,
  NS_HAT_TOP_RIGHT    = 0x01,
  NS_HAT_RIGHT        = 0x02,
  NS_HAT_BOTTOM_RIGHT = 0x03,
  NS_HAT_BOTTOM       = 0x04,
  NS_HAT_BOTTOM_LEFT  = 0x05,
  NS_HAT_LEFT         = 0x06,
  NS_HAT_TOP_LEFT     = 0x07,
  NS_HAT_CENTER       = 0x08,
} ns_input_hat_dir_t;

typedef enum
{
  XI_HAT_TOP          = 0x01,
  XI_HAT_TOP_RIGHT    = 0x02,
  XI_HAT_RIGHT        = 0x03,
  XI_HAT_BOTTOM_RIGHT = 0x04,
  XI_HAT_BOTTOM       = 0x05,
  XI_HAT_BOTTOM_LEFT  = 0x06,
  XI_HAT_LEFT         = 0x07,
  XI_HAT_TOP_LEFT     = 0x08,
  XI_HAT_CENTER       = 0x00,
} xi_input_hat_dir_t;

typedef enum
{
    HAT_MODE_NS,
    HAT_MODE_XI,
} hat_mode_t;

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
            uint8_t padding         : 5;
        };
        uint8_t buttons_system;
    };
} __attribute__ ((packed)) button_data_s;

// Analog input data structure
typedef struct
{
    int lx;
    int ly;
    int rx;
    int ry;
    int lt;
    int rt;
} a_data_s;

// IMU data structure
typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
} __attribute__ ((packed)) imu_data_s;

typedef struct
{
    union
    {
        struct
        {
            // Y and C-Up (N64)
            uint8_t b_y       : 1;

            // X and C-Left (N64)
            uint8_t b_x       : 1;

            uint8_t b_b       : 1;
            uint8_t b_a       : 1;
            uint8_t t_r_sr    : 1;
            uint8_t t_r_sl    : 1;
            uint8_t t_r       : 1;

            // ZR and C-Down (N64)
            uint8_t t_zr      : 1;
        };
        uint8_t right_buttons;
    };
    union
    {
        struct
        {
            // Minus and C-Right (N64)
            uint8_t b_minus     : 1;

            // Plus and Start
            uint8_t b_plus      : 1;

            uint8_t sb_right    : 1;
            uint8_t sb_left     : 1;
            uint8_t b_home      : 1;
            uint8_t b_capture   : 1;
            uint8_t none        : 1;
            uint8_t charge_grip_active : 1;
        };
        uint8_t shared_buttons;
    };
    union
    {
        struct
        {
            uint8_t d_down    : 1;
            uint8_t d_up      : 1;
            uint8_t d_right   : 1;
            uint8_t d_left    : 1;
            uint8_t t_l_sr    : 1;
            uint8_t t_l_sl    : 1;
            uint8_t t_l       : 1;

            // ZL and Z (N64)
            uint8_t t_zl      : 1;

        };
        uint8_t left_buttons;
    };

    uint16_t ls_x;
    uint16_t ls_y;
    uint16_t rs_x;
    uint16_t rs_y;

} __attribute__ ((packed)) sw_input_s;

#endif
