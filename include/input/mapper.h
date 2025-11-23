#ifndef HOJA_MAPPER_H
#define HOJA_MAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "settings_shared_types.h"

#define MAPPER_BUTTON_SET(down, code) (down ? (1<<code) : 0)
#define MAPPER_BUTTON_DOWN(inputs, code) (((inputs & (1 << code)) != 0) ? true : false)

#define MAPPER_INPUT_COUNT 36 // Maximum number of unique inputs

// The mapper is a system that allows for a very broad range of mappings for input and output.
// You can map digital inputs to analog outputs, and map analog inputs to digital outputs. 
// 6 analog inputs and 6 analog outputs are supported. 

typedef enum 
{
    MAPPER_INPUT_TYPE_UNUSED, // Input is disabled
    MAPPER_INPUT_TYPE_DIGITAL, // Binary off or on
    MAPPER_INPUT_TYPE_HOVER, // Analog input 0-4095 (12 bits)
    MAPPER_INPUT_TYPE_JOYSTICK, // Analog input 0-2048 (half of 12 bit range)
} mapper_input_type_t;

typedef enum 
{
    INPUT_CODE_UNUSED = -1,
    INPUT_CODE_SOUTH,
    INPUT_CODE_EAST, 
    INPUT_CODE_WEST,
    INPUT_CODE_NORTH,
    INPUT_CODE_UP,
    INPUT_CODE_DOWN,
    INPUT_CODE_LEFT,
    INPUT_CODE_RIGHT,
    INPUT_CODE_SL,
    INPUT_CODE_SR,
    INPUT_CODE_LB,
    INPUT_CODE_RB,
    INPUT_CODE_LT,
    INPUT_CODE_RT,
    INPUT_CODE_LP1,
    INPUT_CODE_RP1,
    INPUT_CODE_START,
    INPUT_CODE_SELECT,
    INPUT_CODE_HOME,
    INPUT_CODE_SHARE,
    INPUT_CODE_LP2,
    INPUT_CODE_RP2,
    INPUT_CODE_TP1,
    INPUT_CODE_TP2,
    INPUT_CODE_MISC3,
    INPUT_CODE_MISC4,
    INPUT_CODE_LT_ANALOG,
    INPUT_CODE_RT_ANALOG,
    INPUT_CODE_LX_RIGHT,
    INPUT_CODE_LX_LEFT,
    INPUT_CODE_LY_UP,
    INPUT_CODE_LY_DOWN,
    INPUT_CODE_RX_RIGHT,
    INPUT_CODE_RX_LEFT,
    INPUT_CODE_RY_UP,
    INPUT_CODE_RY_DOWN,
    INPUT_CODE_MAX
} mapper_input_code_t;

typedef enum 
{
    MAPPER_OUTPUT_DISABLED,
    MAPPER_OUTPUT_DIGITAL,
    MAPPER_OUTPUT_TRIGGER_L,
    MAPPER_OUTPUT_TRIGGER_R,
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN, 
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN,
} mapper_output_type_t;

typedef enum 
{
    SWITCH_CODE_UNUSED = -1,
    SWITCH_CODE_A,
    SWITCH_CODE_B,
    SWITCH_CODE_X,
    SWITCH_CODE_Y,
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

mapper_output_type_t _switch_output_types[SWITCH_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // LZ
    MAPPER_OUTPUT_DIGITAL, // RZ
    MAPPER_OUTPUT_DIGITAL, // PLUS
    MAPPER_OUTPUT_DIGITAL, // MINUS
    MAPPER_OUTPUT_DIGITAL, // HOME
    MAPPER_OUTPUT_DIGITAL, // CAPTURE
    MAPPER_OUTPUT_DIGITAL, // LS Click
    MAPPER_OUTPUT_DIGITAL, // RS Click
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN, 
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN
};

typedef enum 
{
    SNES_CODE_UNUSED = -1,
    SNES_CODE_A,
    SNES_CODE_B,
    SNES_CODE_X,
    SNES_CODE_Y,
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

mapper_output_type_t _snes_output_types[SNES_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Select
};

typedef enum 
{
    N64_CODE_UNUSED = -1,
    N64_CODE_A,
    N64_CODE_B,
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

mapper_output_type_t _n64_output_types[SWITCH_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // CUP
    MAPPER_OUTPUT_DIGITAL, // CDOWN
    MAPPER_OUTPUT_DIGITAL, // CLEFT
    MAPPER_OUTPUT_DIGITAL, // CRIGHT
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // Z
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN, 
};

typedef enum 
{
    GAMECUBE_CODE_UNUSED = -1,
    GAMECUBE_CODE_A,
    GAMECUBE_CODE_B,
    GAMECUBE_CODE_X,
    GAMECUBE_CODE_Y,
    GAMECUBE_CODE_UP,
    GAMECUBE_CODE_DOWN,
    GAMECUBE_CODE_LEFT,
    GAMECUBE_CODE_RIGHT,
    GAMECUBE_CODE_START,
    GAMECUBE_CODE_Z,
    GAMECUBE_CODE_L, 
    GAMECUBE_CODE_R,
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

mapper_output_type_t _gamecube_output_types[GAMECUBE_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Z
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_TRIGGER_L, // L Analog
    MAPPER_OUTPUT_TRIGGER_R, // R Analog
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN, 
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN
};

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

mapper_input_type_t _xinput_output_types[XINPUT_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // LB
    MAPPER_OUTPUT_DIGITAL, // RB
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Back
    MAPPER_OUTPUT_DIGITAL, // Guide
    MAPPER_OUTPUT_DIGITAL, // LS
    MAPPER_OUTPUT_DIGITAL, // RS
    MAPPER_OUTPUT_TRIGGER_L,
    MAPPER_OUTPUT_TRIGGER_R,
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN,
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN,
};

typedef enum 
{
    SINPUT_CODE_UNUSED = -1,
    SINPUT_CODE_SOUTH,
    SINPUT_CODE_EAST,
    SINPUT_CODE_WEST,
    SINPUT_CODE_NORTH,
    SINPUT_CODE_UP,
    SINPUT_CODE_DOWN,
    SINPUT_CODE_LEFT,
    SINPUT_CODE_RIGHT,
    SINPUT_CODE_LS, // Stick left
    SINPUT_CODE_RS, // Stick right
    SINPUT_CODE_LB,
    SINPUT_CODE_RB,
    SINPUT_CODE_LT, // Left trigger digital
    SINPUT_CODE_RT, // Right trigger digital
    SINPUT_CODE_LP_1, // Left paddle 1
    SINPUT_CODE_RP_1, // Right paddle 1
    SINPUT_CODE_START,
    SINPUT_CODE_SELECT,
    SINPUT_CODE_GUIDE,
    SINPUT_CODE_SHARE,
    SINPUT_CODE_LP_2, // Left paddle 2
    SINPUT_CODE_RP_2, // Right paddle 2
    SINPUT_CODE_TP_1, // Touchpad 1
    SINPUT_CODE_TP_2, // Touchpad 2
    SINPUT_CODE_MISC_3, // Misc 3 (Power)
    SINPUT_CODE_MISC_4, // Misc 4
    SINPUT_CODE_MISC_5, // Misc 5
    SINPUT_CODE_MISC_6, // Misc 6 
    SINPUT_CODE_LT_ANALOG,
    SINPUT_CODE_RT_ANALOG,
    SINPUT_CODE_LX_RIGHT,
    SINPUT_CODE_LX_LEFT,
    SINPUT_CODE_LY_UP,
    SINPUT_CODE_LY_DOWN,
    SINPUT_CODE_RX_RIGHT,
    SINPUT_CODE_RX_LEFT,
    SINPUT_CODE_RY_UP,
    SINPUT_CODE_RY_DOWN,
    SINPUT_CODE_MAX,
} mapper_sinput_code_t;

mapper_input_type_t _sinput_output_types[SINPUT_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // SOUTH
    MAPPER_OUTPUT_DIGITAL, // EAST
    MAPPER_OUTPUT_DIGITAL, // WEST
    MAPPER_OUTPUT_DIGITAL, // NORTH
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // Stick left
    MAPPER_OUTPUT_DIGITAL, // Stick right
    MAPPER_OUTPUT_DIGITAL, // Left bumper
    MAPPER_OUTPUT_DIGITAL, // Right bumper
    MAPPER_OUTPUT_DIGITAL, // Left trigger digital
    MAPPER_OUTPUT_DIGITAL, // Right trigger digital
    MAPPER_OUTPUT_DIGITAL, // Left paddle 1
    MAPPER_OUTPUT_DIGITAL, // Right paddle 1
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Select
    MAPPER_OUTPUT_DIGITAL, // Guide
    MAPPER_OUTPUT_DIGITAL, // Share
    MAPPER_OUTPUT_DIGITAL, // Left paddle 2
    MAPPER_OUTPUT_DIGITAL, // Right paddle 2
    MAPPER_OUTPUT_DIGITAL, // Touchpad 1
    MAPPER_OUTPUT_DIGITAL, // Touchpad 2
    MAPPER_OUTPUT_DIGITAL, // Misc 3
    MAPPER_OUTPUT_DIGITAL, // Misc 4
    MAPPER_OUTPUT_DIGITAL, // Misc 5
    MAPPER_OUTPUT_DIGITAL, // Misc 6 
    MAPPER_OUTPUT_TRIGGER_L,
    MAPPER_OUTPUT_TRIGGER_R,
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN,
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN,
};

#define MAPPER_PROFILE_SIZE sizeof(mapper_profile_s)

typedef struct 
{
    uint32_t digital_inputs;
    uint16_t joysticks_raw[8];
    int16_t joysticks_combined[4]; // Range of -2048 to 2048 with 0 center
 
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
    uint16_t inputs[MAPPER_INPUT_COUNT];
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
} nu_mapper_input_s;

typedef struct 
{
    uint32_t digital_input;
    uint16_t triggers[2];
    int16_t  joysticks[4];
} mapper_output_s;

typedef struct 
{
    int16_t lx_threshold;
    int16_t ly_threshold;
    int16_t rx_threshold;
    int16_t ry_threshold;
    uint16_t lt_threshold;
    uint16_t rt_threshold;
} mapper_global_analog_s;

typedef enum 
{
    MAPPER_OUTPUT_MODE_DEFAULT,
    MAPPER_OUTPUT_MODE_RAPID,
    MAPPER_OUTPUT_MODE_THRESHOLD,
} mapper_output_mode_t;

typedef struct 
{
    uint8_t  output_mode;
    uint16_t static_output_val;
    uint16_t threshold_delta_val;
} mapper_output_cfg_s;

#define MAPPER_PROFILE_GLOBAL_ANALOG_SIZE sizeof(mapper_global_analog_s)

//void mapper_config_command(remap_cmd_t cmd, webreport_cmd_confirm_t cb);
void mapper_init();
mapper_input_s* mapper_get_input();

#endif