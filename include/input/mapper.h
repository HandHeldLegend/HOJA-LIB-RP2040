#ifndef HOJA_MAPPER_H
#define HOJA_MAPPER_H

#include <stdint.h>
#include <stdbool.h>
typedef void (*mapper_check_cb)(void);

typedef void (*mapper_update_button_cb)(bool down);
typedef void (*mapper_update_joystick_cb)(int16_t);
typedef void (*mapper_update_trigger_cb)(uint16_t);

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

typedef struct 
{
    int8_t map[36]; // 32 used, but we have a reserved
} mapper_profile_s;

#define MAPPER_PROFILE_SIZE sizeof(mapper_profile_s)

typedef struct 
{
    uint32_t digital_inputs;
    uint16_t joysticks_raw[8];
    int16_t joysticks_combined[4];
    uint16_t triggers[2];
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

void mapper_process();

#endif