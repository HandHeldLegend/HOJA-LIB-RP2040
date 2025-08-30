#ifndef HOJA_MAPPER_H
#define HOJA_MAPPER_H

#include <stdint.h>
#include <stdbool.h>

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
    MAPPER_CODE_LX_UP,
    MAPPER_CODE_LX_DOWN,
    MAPPER_CODE_LX_RIGHT,
    MAPPER_CODE_LX_LEFT,
    MAPPER_CODE_RX_UP,
    MAPPER_CODE_RX_DOWN,
    MAPPER_CODE_RY_RIGHT,
    MAPPER_CODE_RY_DOWN,
    MAPPER_CODE_LT_ANALOG,
    MAPPER_CODE_RT_ANALOG
} mapper_code_t;

typedef struct 
{
    uint32_t digital_inputs;
    int16_t lx;
    int16_t ly;
    int16_t rx;
    int16_t ry;
    uint16_t lt;
    uint16_t rt;
} mapper_input_s;

typedef struct 
{
    int16_t lx_storage;
    int16_t ly_threshold;
    int16_t rx_threshold;
    int16_t ry_threshold;
    uint16_t lt_threshold;
    uint16_t rt_threshold;
} mapper_global_analog_s;

#define MAPPER_PROFILE_GLOBAL_ANALOG_SIZE sizeof(mapper_global_analog_s)

typedef struct 
{
    int8_t a_btn;
    int8_t b_btn;

    int8_t d_up;
    int8_t d_down;
    int8_t d_left;
    int8_t d_right;

    int8_t c_up;
    int8_t c_down;
    int8_t c_left;
    int8_t c_right;

    int8_t z_btn;
    int8_t l_btn;
    int8_t r_btn;
    int8_t start_btn;
} mapper_profile_n64_s;

#define MAPPER_PROFILE_N64_SIZE sizeof(mapper_profile_n64_s)

typedef struct 
{
    int8_t a_btn;
    int8_t b_btn;
    int8_t x_btn;
    int8_t y_btn;

    int8_t d_up;
    int8_t d_down;
    int8_t d_left;
    int8_t d_right;

    int8_t start_btn;
    int8_t select_btn;
    int8_t l_btn;
    int8_t r_btn;
} mapper_profile_snes_s;

#define MAPPER_PROFILE_SNES_SIZE sizeof(mapper_profile_snes_s)

typedef struct 
{
    int8_t a_btn;
    int8_t b_btn;
    int8_t x_btn;
    int8_t y_btn;

    int8_t d_up;
    int8_t d_down;
    int8_t d_left;
    int8_t d_right;

    int8_t z_btn;
    int8_t l_btn;
    int8_t r_btn;
    int8_t start_btn;

    int8_t sx_up;
    int8_t sx_down;
    int8_t sy_right;
    int8_t sy_left;

    int8_t cx_up;
    int8_t cx_down;
    int8_t cy_right;
    int8_t cy_left;
    
    int8_t l_analog;
    int8_t r_analog;

    int8_t l_analog_static;
    int8_t r_analog_static;
} mapper_profile_gamecube_s;

#define MAPPER_PROFILE_N64_SIZE sizeof(mapper_profile_n64_s)

typedef struct 
{
    int8_t a_btn;
    int8_t b_btn;
    int8_t x_btn;
    int8_t y_btn;

    int8_t d_up;
    int8_t d_down;
    int8_t d_left;
    int8_t d_right;

    int8_t lb_btn;
    int8_t rb_btn;
    int8_t sl_btn;
    int8_t sr_btn;

    int8_t guide_btn;
    int8_t menu_btn;
    int8_t back_btn;
    int8_t reserved;

    int8_t lx_up;
    int8_t lx_down;
    int8_t ly_right;
    int8_t ly_left;

    int8_t rx_up;
    int8_t rx_down;
    int8_t ry_right;
    int8_t ry_left;
    
    int8_t l_analog;
    int8_t r_analog;

    int8_t l_analog_static;
    int8_t r_analog_static;
} mapper_profile_xinput_s;

#define MAPPER_PROFILE_XINPUT_SIZE sizeof(mapper_profile_xinput_s)

typedef struct 
{
    int8_t a_btn;
    int8_t b_btn;
    int8_t x_btn;
    int8_t y_btn;

    int8_t d_up;
    int8_t d_down;
    int8_t d_left;
    int8_t d_right;

    int8_t l_btn;
    int8_t r_btn;
    int8_t zl_btn;
    int8_t zr_btn;

    int8_t sl_btn;
    int8_t sr_btn;

    int8_t plus_btn;
    int8_t minus_btn;
    int8_t home_btn;
    int8_t capture_btn;

    int8_t lx_up;
    int8_t lx_down;
    int8_t ly_right;
    int8_t ly_left;

    int8_t rx_up;
    int8_t rx_down;
    int8_t ry_right;
    int8_t ry_left;
} mapper_profile_switch_s;

#define MAPPER_PROFILE_SWITCH_SIZE sizeof(mapper_profile_switch_s)

#endif