#ifndef UTILITIES_SETTINGS_H
#define UTILITIES_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

#include "devices/devices.h"

#include "input/remap.h"

#include "devices/haptics.h"

// Analog config commands
typedef enum 
{
    ANALOG_CMD_SET_CENTER, 
    ANALOG_CMD_SET_DISTANCE, 
    ANALOG_CMD_SET_ANGLE, 
    ANALOG_CMD_SET_SUBANGLE, 
    ANALOG_CMD_SET_SNAPBACK, 
    ANALOG_CMD_SET_DEADZONE, 
    ANALOG_CMD_SET_INVERT, 
} analog_cmd_t;

// Trigger config commands 
typedef enum 
{
    TRIGGER_CMD_SET_BOUNDS, 
    TRIGGER_CMD_SET_DEADZONE, 
    TRIGGER_CMD_SET_ENABLE, 
} trigger_cmd_t;

// Gamepad config commands
typedef enum 
{
    GAMEPAD_CMD_SET_DEFAULT_MODE, 
    GAMEPAD_CMD_RESET_TO_BOOTLOADER,
    GAMEPAD_CMD_ENABLE_BLUETOOTH_UPLOAD,
    GAMEPAD_CMD_SAVE_ALL,
} gamepad_cmd_t;

// IMU config commands
typedef enum 
{
    IMU_CMD_SET_OFFSETS,
} imu_cmd_t;

// Button config commands
typedef enum 
{
    BUTTON_CMD_SET_REMAP,
    BUTTON_CMD_SET_UNSET,
} button_cmd_t;

// Haptic config commands
typedef enum 
{
    HAPTIC_CMD_SET_STRENGTH, 
} haptic_cmd_t;

typedef enum 
{
    RGB_CMD_SET_MODE,
    RGB_CMD_SET_GROUP, 
    RGB_CMD_SET_SPEED, 
} rgb_cmd_t;

typedef union
{
    struct
    {
        int center : 31;
        bool invert : 1;
    };
    int value;
    
} analog_calibration_u;

typedef union 
{
    struct
    {
        bool     disabled : 1;
        uint16_t lower : 15;
        uint16_t upper : 16;
    };
    uint32_t value;
} trigger_calibration_u;

#endif
