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
    ANALOG_CMD_SET_CENTERS, 
    ANALOG_CMD_CALIBRATE_START, 
    ANALOG_CMD_CALIBRATE_STOP, 
    ANALOG_CMD_CAPTURE_ANGLE, 
    ANALOG_CMD_UPDATE_ANGLE, 
    ANALOG_CMD_SET_SNAPBACK, 
    ANALOG_CMD_SET_DEADZONES, 
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

// Byte sizes of our various blocks
#define GAMEPAD_CFB_SIZE    32 
#define REMAP_CFB_SIZE      32 
#define RGB_CFB_SIZE        256 
#define ANALOG_CFB_SIZE     1024 
#define TRIGGER_CFB_SIZE    32 
#define IMU_CFB_SIZE        32 
#define HAPTIC_CFB_SIZE     8 
#define USER_CFB_SIZE       64

// Byte size of all combined blocks
#define TOTAL_CFB_SIZE (GAMEPAD_CFB_SIZE+REMAP_CFB_SIZE+RGB_CFB_SIZE+\
                        ANALOG_CFB_SIZE+TRIGGER_CFB_SIZE+IMU_CFB_SIZE+HAPTIC_CFB_SIZE+\
                        USER_CFB_SIZE)

typedef union 
{
    struct 
    {
        uint8_t user_name[24];
        uint8_t reserved[USER_CFB_SIZE-24];
    };
    uint8_t user_configuration_block[USER_CFB_SIZE];
} user_config_u;

typedef union 
{
    struct 
    {
        uint8_t     haptic_strength;
        uint8_t     reserved[HAPTIC_CFB_SIZE-1];
    };
    uint8_t haptic_configuration_block[HAPTIC_CFB_SIZE];
} haptic_config_u;

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

typedef union 
{
    struct 
    {
        int first_distance;
        int16_t offsets[63];
    };
    uint8_t values[130];
} analog_packed_distances_u; // 130 bytes

typedef struct
{
  float input;    // Input Angle
  float output;   // Output Angle
  int distance; // Distance to this input angle maximum for hard polygonal shape stick gates
} angle_map_s;

typedef struct 
{
    struct 
    {
        uint8_t     analog_config_version;
        uint8_t     lx_invert : 1;
        uint16_t    lx_center : 15;
        uint8_t     ly_invert : 1;
        uint16_t    ly_center : 15;
        uint8_t     rx_invert : 1;
        uint16_t    rx_center : 15;
        uint8_t     ry_invert : 1;
        uint16_t    ry_center : 15; // 9 bytes up to here
        analog_packed_distances_u l_packed_distances;
        analog_packed_distances_u r_packed_distances; // 269 bytes up to here
        angle_map_s l_angle_maps[16]; // 461
        angle_map_s r_angle_maps[16]; // 473
        uint8_t     l_scaler_type; // 474
        uint8_t     r_scaler_type; // 475
        uint8_t     reserved[ANALOG_CFB_SIZE-475];
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