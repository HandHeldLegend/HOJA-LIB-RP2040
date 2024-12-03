#ifndef SETTINGS_SHARED_TYPES_H
#define SETTINGS_SHARED_TYPES_H

#include <stdint.h>

// Analog config commands
typedef enum 
{
    ANALOG_CMD_SET_CENTERS, 
    ANALOG_CMD_SET_DISTANCES, 
    ANALOG_CMD_SET_ANGLES, 
    ANALOG_CMD_SET_SNAPBACK, 
    ANALOG_CMD_SET_DEADZONES, 
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
    IMU_CMD_CALIBRATE,
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

typedef void (*setting_callback_t)(const uint8_t *data, uint16_t size);

#define GAMEPAD_CFB_SIZE    64 
#define REMAP_CFB_SIZE      64 
#define RGB_CFB_SIZE        256 
#define ANALOG_CFB_SIZE     1024 
#define TRIGGER_CFB_SIZE    64 
#define IMU_CFB_SIZE        64 
#define HAPTIC_CFB_SIZE     8 

#define TOTAL_CFB_SIZE (GAMEPAD_CFB_SIZE+REMAP_CFB_SIZE+RGB_CFB_SIZE+\
                        ANALOG_CFB_SIZE+TRIGGER_CFB_SIZE+IMU_CFB_SIZE+HAPTIC_CFB_SIZE)

typedef union 
{
    struct 
    {
        uint8_t     imu_config_version;
        uint8_t     imu_a_gyro_config[3];
        uint8_t     imu_a_accel_config[3];
        uint8_t     imu_b_gyro_config[3];
        uint8_t     imu_b_accel_config[3];
        uint8_t     reserved[IMU_CFB_SIZE-25];
    };
    uint8_t imu_configuration_block[IMU_CFB_SIZE];
} imu_config_u;

typedef union 
{
    struct 
    {
        uint8_t     trigger_config_version;
        uint32_t    left_config;
        uint32_t    right_config;
        uint8_t     left_static_output_value;
        uint8_t     right_static_output_value;
        uint8_t     reserved[TRIGGER_CFB_SIZE-11];
    };
    uint8_t trigger_configuration_block[TRIGGER_CFB_SIZE];
} trigger_config_u;

typedef struct 
{
    struct 
    {
        uint8_t     analog_config_version;
        uint16_t    lx_config; // Center and invert value
        uint16_t    ly_config;
        uint16_t    rx_config;
        uint16_t    ry_config;
        uint8_t     reserved[ANALOG_CFB_SIZE-9];
    };
    uint8_t analog_configuration_block[ANALOG_CFB_SIZE];
} analog_config_u;

typedef union 
{
    struct 
    {
        uint8_t rgb_config_version;
        uint8_t  rgb_mode;
        uint8_t  rgb_speed_factor;
        uint32_t rgb_colors[32]; // Store 32 RGB colors
        uint8_t  reserved[RGB_CFB_SIZE-3-(32*4)];
    };
    uint8_t rgb_configuration_block[RGB_CFB_SIZE];
} rgb_config_u;

typedef union 
{
    struct 
    {
        uint8_t gamepad_config_version;
        uint8_t switch_mac_address[6];
        uint8_t gamepad_default_mode;
        uint8_t sp_function_mode;
        uint8_t dpad_socd_mode;
        uint8_t reserved[GAMEPAD_CFB_SIZE-10];
    };
    uint8_t gamepad_configuration_block[GAMEPAD_CFB_SIZE];
} gamepad_config_u;

typedef union
{
    struct 
    {
        uint8_t remap_config_version;
        // SNES, N64, GameCube, Switch, XInput
        uint16_t profiles[12]; // Reserve space for 12 profiles
        uint16_t disabled[12]; // 12 disabled options (tells remap which buttons to disable)
        uint8_t  reserved[REMAP_CFB_SIZE-25];
    };
    uint8_t remap_configuration_block[REMAP_CFB_SIZE];
} remap_config_u;           

#endif