#ifndef INPUT_SHARED_TYPES_H
#define INPUT_SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Button data laid out in bit order matching mapper_code_t */
typedef struct
{
    union
    {
        struct
        {
            // Face buttons
            uint32_t button_south        : 1; // MAPPER_CODE_SOUTH
            uint32_t button_east         : 1; // MAPPER_CODE_EAST
            uint32_t button_west         : 1; // MAPPER_CODE_WEST
            uint32_t button_north        : 1; // MAPPER_CODE_NORTH

            // D-Pad
            uint32_t dpad_up             : 1; // MAPPER_CODE_UP
            uint32_t dpad_down           : 1; // MAPPER_CODE_DOWN
            uint32_t dpad_left           : 1; // MAPPER_CODE_LEFT
            uint32_t dpad_right          : 1; // MAPPER_CODE_RIGHT

            // Shoulder / trigger buttons (digital)
            uint32_t trigger_l           : 1; // MAPPER_CODE_LB
            uint32_t trigger_r           : 1; // MAPPER_CODE_RB
            uint32_t trigger_zl          : 1; // MAPPER_CODE_LT
            uint32_t trigger_zr          : 1; // MAPPER_CODE_RT

            // Menu buttons
            uint32_t button_plus         : 1; // MAPPER_CODE_START
            uint32_t button_minus        : 1; // MAPPER_CODE_SELECT
            uint32_t button_home         : 1; // MAPPER_CODE_HOME
            uint32_t button_capture      : 1; // MAPPER_CODE_CAPTURE

            // Stick clicks
            uint32_t button_stick_left   : 1; // MAPPER_CODE_LS
            uint32_t button_stick_right  : 1; // MAPPER_CODE_RS

            // Grip buttons
            uint32_t trigger_gl          : 1; // MAPPER_CODE_LG_UPPER
            uint32_t trigger_gr          : 1; // MAPPER_CODE_RG_UPPER
            uint32_t trigger_gl2         : 1; // MAPPER_CODE_LG_LOWER
            uint32_t trigger_gr2         : 1; // MAPPER_CODE_RG_LOWER

            // Unused data
            uint32_t padding : 10; // Analog data
        };

        uint32_t buttons; // raw 32-bit access
    };

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

    
} button_data_s;

#define BUTTON_DATA_SIZE sizeof(button_data_s)

typedef struct 
{
    uint16_t left_analog;
    uint16_t right_analog;
} trigger_data_s;

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
    SNAPBACK_TYPE_AUTO,
    SNAPBACK_TYPE_LPF,
    SNAPBACK_TYPE_DISABLED,
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