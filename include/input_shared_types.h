#ifndef INPUT_SHARED_TYPES_H
#define INPUT_SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define MAPPER_INPUT_COUNT 36 // Maximum number of unique inputs

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
    SWITCH_CODE_ZL, 
    SWITCH_CODE_ZR,
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

#define MAPPER_PROFILE_SIZE sizeof(mapper_profile_s)

typedef struct 
{
    uint16_t inputs[MAPPER_INPUT_COUNT];
    union
    {
        struct 
        {
            uint8_t button_sync : 1;
            uint8_t button_safemode : 1; 
            uint8_t button_shipping : 1; 
        };
        uint8_t buttons_system;
    };
} mapper_input_s;

// Analog input data structure
typedef struct
{
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;
    float langle;       // Current value angle
    uint16_t ldistance;    // Current value distance
    uint16_t ltarget;      // Current value target distance (scaling)
    float rangle;
    uint16_t rdistance;
    uint16_t rtarget;
} analog_data_s;

typedef struct 
{
    int16_t x;
    int16_t y;
    float angle;
    uint16_t distance;  // Distance from center
    uint16_t target;    // Target distance (scaling)
    uint8_t axis_idx;
} analog_axis_s;

typedef struct 
{
    float    angle;
    bool     crossover;
    uint16_t distance;
    uint16_t target;
} analog_meta_s;

typedef enum
{
    ANALOG_SCALER_ROUND,
    ANALOG_SCALER_POLYGON,
} analog_scaler_t;

typedef enum
{
    CALIBRATE_START,
    CALIBRATE_CANCEL,
    CALIBRATE_SAVE,
} calibrate_set_t;

typedef enum 
{
    SNAPBACK_TYPE_DISABLED,
    SNAPBACK_TYPE_ZERO,
    SNAPBACK_TYPE_POST,
} snapback_type_t;

// IMU data structure
typedef struct
{
    union
    {
        struct
        {
            uint8_t ax_8l : 8;
            uint8_t ax_8h : 8;
        };
        int16_t ax;
    };

    union
    {
        struct
        {
            uint8_t ay_8l : 8;
            uint8_t ay_8h : 8;
        };
        int16_t ay;
    };
    
    union
    {
        struct
        {
            uint8_t az_8l : 8;
            uint8_t az_8h : 8;
        };
        int16_t az;
    };
    
    union
    {
        struct
        {
            uint8_t gx_8l : 8;
            uint8_t gx_8h : 8;
        };
        int16_t gx;
    };

    union
    {
        struct
        {
            uint8_t gy_8l : 8;
            uint8_t gy_8h : 8;
        };
        int16_t gy;
    };
    
    union
    {
        struct
        {
            uint8_t gz_8l : 8;
            uint8_t gz_8h : 8;
        };
        int16_t gz;
    };
    
    bool retrieved;
    uint64_t timestamp;
} imu_data_s;

typedef struct
{
    union
    {
        float raw[4];
        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
    int16_t ax;
    int16_t ay;
    int16_t az;
    uint64_t timestamp_ms; // Timestamp in ms 
} quaternion_s;

#endif