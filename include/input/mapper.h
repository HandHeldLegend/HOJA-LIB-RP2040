#ifndef HOJA_MAPPER_H
#define HOJA_MAPPER_H

#include <stdint.h>
#include <stdbool.h>

#define MAPPER_BUTTON_SET(down, code) (down ? (1<<code) : 0)
#define MAPPER_BUTTON_DOWN(inputs, code) (((inputs & (1 << code)) != 0) ? true : false)

// The mapper is a system that allows for a very broad range of mappings for input and output.
// You can map digital inputs to analog outputs, and map analog inputs to digital outputs. 
// 6 analog inputs and 6 analog outputs are supported. 

typedef enum 
{
    MAPPER_CODE_UNUSED = -1,
    MAPPER_CODE_SOUTH,
    MAPPER_CODE_EAST,
    MAPPER_CODE_WEST,
    MAPPER_CODE_NORTH,
    MAPPER_CODE_UP,
    MAPPER_CODE_DOWN,
    MAPPER_CODE_LEFT,
    MAPPER_CODE_RIGHT,
    MAPPER_CODE_LB, // Bumper L
    MAPPER_CODE_RB, // Bumper R
    MAPPER_CODE_LT, // Left Trigger (Digital ZL)
    MAPPER_CODE_RT, // Right Trigger (Digital ZR)
    MAPPER_CODE_START,
    MAPPER_CODE_SELECT,
    MAPPER_CODE_HOME,
    MAPPER_CODE_CAPTURE,
    MAPPER_CODE_LS, // Stick buttons
    MAPPER_CODE_RS,
    MAPPER_CODE_LG_UPPER, // Grip button left upper
    MAPPER_CODE_RG_UPPER, // Grip button right upper
    MAPPER_CODE_LG_LOWER, // Grip button left lower
    MAPPER_CODE_RG_LOWER, // Grip button right lower
    MAPPER_CODE_LT_ANALOG,
    MAPPER_CODE_RT_ANALOG,
    MAPPER_CODE_LX_RIGHT,
    MAPPER_CODE_LX_LEFT,
    MAPPER_CODE_LY_UP,
    MAPPER_CODE_LY_DOWN,
    MAPPER_CODE_RX_RIGHT,
    MAPPER_CODE_RX_LEFT,
    MAPPER_CODE_RY_UP,
    MAPPER_CODE_RY_DOWN,
    MAPPER_CODE_MAX,
} mapper_code_t;

#define MAPPER_CODE_IDX_TRIGGER_START  MAPPER_CODE_LT_ANALOG
#define MAPPER_CODE_IDX_JOYSTICK_START MAPPER_CODE_LX_RIGHT

typedef enum 
{
    SWITCH_CODE_UNUSED = -1,
    SWITCH_CODE_B,
    SWITCH_CODE_A,
    SWITCH_CODE_Y,
    SWITCH_CODE_X,
    SWITCH_CODE_UP,
    SWITCH_CODE_DOWN,
    SWITCH_CODE_LEFT,
    SWITCH_CODE_RIGHT,
    SWITCH_CODE_L,
    SWITCH_CODE_R,
    SWITCH_CODE_LZ, 
    SWITCH_CODE_RZ,
    SWITCH_CODE_PLUS,
    SWITCH_CODE_MINUS,
    SWITCH_CODE_HOME,
    SWITCH_CODE_CAPTURE,
    SWITCH_CODE_LS,
    SWITCH_CODE_RS,
    SWITCH_CODE_LX_RIGHT,
    SWITCH_CODE_LX_LEFT,
    SWITCH_CODE_LY_UP,
    SWITCH_CODE_LY_DOWN,
    SWITCH_CODE_RX_RIGHT,
    SWITCH_CODE_RX_LEFT,
    SWITCH_CODE_RY_UP,
    SWITCH_CODE_RY_DOWN,
    SWITCH_CODE_MAX,
} mapper_switch_code_t;

#define SWITCH_CODE_IDX_JOYSTICK_START SWITCH_CODE_LX_RIGHT

typedef enum 
{
    SNES_CODE_UNUSED = -1,
    SNES_CODE_B,
    SNES_CODE_A,
    SNES_CODE_Y,
    SNES_CODE_X,
    SNES_CODE_UP,
    SNES_CODE_DOWN,
    SNES_CODE_LEFT,
    SNES_CODE_RIGHT,
    SNES_CODE_L,
    SNES_CODE_R,
    SNES_CODE_START,
    SNES_CODE_SELECT,
    SNES_CODE_MAX,
} mapper_snes_code_t;

typedef enum 
{
    N64_CODE_UNUSED = -1,
    N64_CODE_B,
    N64_CODE_A,
    N64_CODE_CUP,
    N64_CODE_CDOWN,
    N64_CODE_CLEFT,
    N64_CODE_CRIGHT,
    N64_CODE_UP,
    N64_CODE_DOWN,
    N64_CODE_LEFT,
    N64_CODE_RIGHT,
    N64_CODE_L,
    N64_CODE_R,
    N64_CODE_Z, 
    N64_CODE_START,
    N64_CODE_LX_RIGHT,
    N64_CODE_LX_LEFT,
    N64_CODE_LY_UP,
    N64_CODE_LY_DOWN,
    N64_CODE_MAX,
} mapper_n64_code_t;

#define N64_CODE_IDX_JOYSTICK_START N64_CODE_LX_RIGHT

typedef enum 
{
    GAMECUBE_CODE_UNUSED = -1,
    GAMECUBE_CODE_B,
    GAMECUBE_CODE_A,
    GAMECUBE_CODE_Y,
    GAMECUBE_CODE_X,
    GAMECUBE_CODE_UP,
    GAMECUBE_CODE_DOWN,
    GAMECUBE_CODE_LEFT,
    GAMECUBE_CODE_RIGHT,
    GAMECUBE_CODE_Z,
    GAMECUBE_CODE_L, 
    GAMECUBE_CODE_R,
    GAMECUBE_CODE_START,
    GAMECUBE_CODE_L_ANALOG,
    GAMECUBE_CODE_R_ANALOG,
    GAMECUBE_CODE_LX_RIGHT,
    GAMECUBE_CODE_LX_LEFT,
    GAMECUBE_CODE_LY_UP,
    GAMECUBE_CODE_LY_DOWN,
    GAMECUBE_CODE_RX_RIGHT,
    GAMECUBE_CODE_RX_LEFT,
    GAMECUBE_CODE_RY_UP,
    GAMECUBE_CODE_RY_DOWN,
    GAMECUBE_CODE_MAX,
} mapper_gamecube_code_t;

#define GAMECUBE_CODE_IDX_TRIGGER_START GAMECUBE_CODE_L_ANALOG
#define GAMECUBE_CODE_IDX_JOYSTICK_START GAMECUBE_CODE_LX_RIGHT

typedef enum 
{
    XINPUT_CODE_UNUSED = -1,
    XINPUT_CODE_A,
    XINPUT_CODE_B,
    XINPUT_CODE_X,
    XINPUT_CODE_Y,
    XINPUT_CODE_UP,
    XINPUT_CODE_DOWN,
    XINPUT_CODE_LEFT,
    XINPUT_CODE_RIGHT,
    XINPUT_CODE_LB,
    XINPUT_CODE_RB,
    XINPUT_CODE_START,
    XINPUT_CODE_BACK,
    XINPUT_CODE_GUIDE,
    XINPUT_CODE_LS,
    XINPUT_CODE_RS,
    XINPUT_CODE_LT_ANALOG, 
    XINPUT_CODE_RT_ANALOG,
    XINPUT_CODE_LX_RIGHT,
    XINPUT_CODE_LX_LEFT,
    XINPUT_CODE_LY_UP,
    XINPUT_CODE_LY_DOWN,
    XINPUT_CODE_RX_RIGHT,
    XINPUT_CODE_RX_LEFT,
    XINPUT_CODE_RY_UP,
    XINPUT_CODE_RY_DOWN,
    XINPUT_CODE_MAX,
} mapper_xinput_code_t;

#define XINPUT_CODE_IDX_TRIGGER_START XINPUT_CODE_LT_ANALOG
#define XINPUT_CODE_IDX_JOYSTICK_START XINPUT_CODE_LX_RIGHT

typedef struct 
{
    union
    {
        struct
        {
            int8_t south;
            int8_t east;
            int8_t west;
            int8_t north;
            int8_t up;
            int8_t down;
            int8_t left;
            int8_t right;
            int8_t lb;
            int8_t rb;
            int8_t lt;
            int8_t rt;
            int8_t start;
            int8_t select;
            int8_t home;
            int8_t capture;
            int8_t ls;
            int8_t rs;
            int8_t lg_upper;
            int8_t rg_upper;
            int8_t lg_lower;
            int8_t rg_lower;
            int8_t lt_analog;
            int8_t rt_analog;
            int8_t lx_right;
            int8_t lx_left;
            int8_t ly_up;
            int8_t ly_down;
            int8_t rx_right;
            int8_t rx_left;
            int8_t ry_up;
            int8_t ry_down;
        };
        int8_t map[36]; // 32 used, but we have a reserved
    };
} mapper_profile_s; 

#define MAPPER_PROFILE_SIZE sizeof(mapper_profile_s)

typedef struct 
{
    uint32_t digital_inputs;
    uint16_t joysticks_raw[8];
    int16_t joysticks_combined[4];
    uint16_t triggers[2];
    union
    {
        struct 
        {
            uint8_t button_sync : 1;
            uint8_t button_safemode : 1; // MAPPER_CODE_LG_LOWER
            uint8_t button_shipping : 1; // MAPPER_CODE_RG_LOWER
        };
        uint8_t buttons_system;
    };
} mapper_input_s;

typedef struct 
{
    int16_t lx_threshold;
    int16_t ly_threshold;
    int16_t rx_threshold;
    int16_t ry_threshold;
    uint16_t lt_threshold;
    uint16_t rt_threshold;
} mapper_global_analog_s;

#define MAPPER_PROFILE_GLOBAL_ANALOG_SIZE sizeof(mapper_global_analog_s)

void mapper_init();
mapper_input_s* mapper_get_input();

#endif