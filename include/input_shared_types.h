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

    int16_t zl_analog;
    int16_t zr_analog;
} button_data_s;

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
    bool lcrossover; // dot product bool indicates crossover
    float rangle;
    uint16_t rdistance;
    uint16_t rtarget;
    bool rcrossover;  
} analog_data_s;

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
    SNAPBACK_TYPE_DISABLED,
    SNAPBACK_TYPE_ZERO,
    SNAPBACK_TYPE_POST,
} snapback_type_t;

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
    //uint32_t timestamp;
    int16_t ax;
    int16_t ay;
    int16_t az;
} quaternion_s;

#endif