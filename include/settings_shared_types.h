#ifndef SETTINGS_SHARED_TYPES_H
#define SETTINGS_SHARED_TYPES_H

#include <stdint.h>

typedef enum 
{
    CFG_BLOCK_GAMEPAD, 
    CFG_BLOCK_REMAP, 
    CFG_BLOCK_ANALOG, 
    CFG_BLOCK_RGB, 
    CFG_BLOCK_TRIGGER, 
    CFG_BLOCK_IMU, 
    CFG_BLOCK_HAPTIC, 
    CFG_BLOCK_USER, 
    CFG_BLOCK_BATTERY,
    CFG_BLOCK_MAX, 
} cfg_block_t;

typedef enum 
{
    GAMEPAD_CMD_SET_DEFAULT_MODE, 
    GAMEPAD_CMD_RESET_TO_BOOTLOADER, 
    GAMEPAD_CMD_ENABLE_BLUETOOTH_UPLOAD, 
    GAMEPAD_CMD_SAVE_ALL, 
} gamepad_cmd_t;

typedef enum 
{
    REMAP_CMD_SET_REMAP,
    REMAP_CMD_SET_UNSET,
} remap_cmd_t;

typedef enum 
{
    ANALOG_CMD_CALIBRATE_START, 
    ANALOG_CMD_CALIBRATE_STOP, 
    ANALOG_CMD_CAPTURE_ANGLE, 
} analog_cmd_t;

typedef enum 
{
    RGB_CMD_SET_MODE, 
    RGB_CMD_SET_GROUP, 
    RGB_CMD_SET_LED,  
    RGB_CMD_SET_SPEED,  
} rgb_cmd_t;

typedef enum 
{
    IMU_CMD_CALIBRATE, 
} imu_cmd_t;

typedef enum 
{
    HAPTIC_CMD_SET_STRENGTH, 
    HAPTIC_CMD_TEST_STRENGTH, 
} haptic_cmd_t;

typedef enum 
{
    TRIGGER_CMD_SET_BOUNDS, 
    TRIGGER_CMD_SET_DEADZONE, 
    TRIGGER_CMD_SET_ENABLE, 
} trigger_cmd_t;

typedef void (*setting_callback_t)(const uint8_t *data, uint16_t size);

#pragma pack(push, 1)
typedef struct 
{
    float charge_level_percent;
    uint8_t reserved[4];
} batteryConfig_s;

typedef struct 
{
    uint8_t user_name[24];
    uint8_t reserved[40];
} userConfig_s;

typedef struct 
{

    uint8_t     haptic_strength;
    uint8_t     reserved[7];
} hapticConfig_s;

typedef struct 
{

    uint8_t     imu_config_version;
    uint8_t     imu_a_gyro_config[3];
    uint8_t     imu_a_accel_config[3];
    uint8_t     imu_b_gyro_config[3];
    uint8_t     imu_b_accel_config[3];
    uint8_t     reserved[19];
} imuConfig_s;

typedef struct 
{
    uint8_t     trigger_config_version;
    uint32_t    left_config;
    uint32_t    right_config;
    uint8_t     left_static_output_value;
    uint8_t     right_static_output_value;
    uint8_t     reserved[21];
} triggerConfig_s;

typedef struct 
{
    int first_distance;
    int16_t offsets[63];
} analogPackedDistances_s;

#define ANALOG_PACKED_DISTANCES_SIZE sizeof(analogPackedDistances_s)

typedef struct
{
  float input;    // Input Angle
  float output;   // Output Angle
  int distance; // Distance to this input angle maximum for hard polygonal shape stick gates
} angleMap_s;

#define ANGLE_MAP_SIZE sizeof(angleMap_s)

typedef struct 
{
        uint8_t     analog_config_version;
        uint16_t    lx_invert : 1;
        uint16_t    lx_center : 15;
        uint16_t    ly_invert : 1;
        uint16_t    ly_center : 15;
        uint16_t    rx_invert : 1;
        uint16_t    rx_center : 15;
        uint16_t    ry_invert : 1;
        uint16_t    ry_center : 15; 
        analogPackedDistances_s l_packed_distances; // SIZE=130
        analogPackedDistances_s r_packed_distances; // SIZE=130
        angleMap_s  l_angle_maps[16]; // SIZE=12
        angleMap_s  r_angle_maps[16]; // SIZE=12
        uint8_t     l_scaler_type; 
        uint8_t     r_scaler_type; 
        uint8_t     reserved[369];
} analogConfig_s;

typedef struct 
{
    uint8_t rgb_config_version;
    uint8_t  rgb_mode;
    uint8_t  rgb_speed_factor;
    uint32_t rgb_colors[32]; // Store 32 RGB colors
    uint8_t  reserved[125];
} rgbConfig_s;

typedef struct 
{
    uint8_t gamepad_config_version;
    uint8_t switch_mac_address[6];
    uint8_t gamepad_default_mode;
    uint8_t sp_function_mode;
    uint8_t dpad_socd_mode;
    uint8_t reserved[22];
} gamepadConfig_s;

typedef struct
{
    uint8_t remap_config_version : 4;
    uint8_t remap_config_setting : 4;
    // SNES, N64, GameCube, Switch, XInput
    uint16_t profiles[12]; // Reserve space for 12 profiles
    uint16_t disabled[12]; // 12 disabled options (tells remap which buttons to disable)
    uint8_t  reserved[15];
} remapConfig_s; 
#pragma pack(pop)

// Byte sizes of our various blocks
#define GAMEPAD_CFB_SIZE    sizeof(gamepadConfig_s) 
#define REMAP_CFB_SIZE      sizeof(remapConfig_s) 
#define RGB_CFB_SIZE        sizeof(rgbConfig_s) 
#define ANALOG_CFB_SIZE     sizeof(analogConfig_s) 
#define TRIGGER_CFB_SIZE    sizeof(triggerConfig_s) 
#define IMU_CFB_SIZE        sizeof(imuConfig_s) 
#define HAPTIC_CFB_SIZE     sizeof(hapticConfig_s) 
#define USER_CFB_SIZE       sizeof(userConfig_s)
#define BATTERY_CFB_SIZE    sizeof(batteryConfig_s)

// Byte size of all combined blocks
#define TOTAL_CFB_SIZE (GAMEPAD_CFB_SIZE+REMAP_CFB_SIZE+RGB_CFB_SIZE+\
                        ANALOG_CFB_SIZE+TRIGGER_CFB_SIZE+IMU_CFB_SIZE+HAPTIC_CFB_SIZE+\
                        USER_CFB_SIZE+BATTERY_CFB_SIZE)

#endif