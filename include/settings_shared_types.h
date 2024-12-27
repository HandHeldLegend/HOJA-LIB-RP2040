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

#define CFG_BLOCK_GAMEPAD_VERSION   0x1
#define CFG_BLOCK_REMAP_VERSION     0x1 
#define CFG_BLOCK_ANALOG_VERSION    0x1
#define CFG_BLOCK_RGB_VERSION       0x1
#define CFG_BLOCK_TRIGGER_VERSION   0x1
#define CFG_BLOCK_IMU_VERSION       0x1
#define CFG_BLOCK_HAPTIC_VERSION    0x1
#define CFG_BLOCK_USER_VERSION      0x1
#define CFG_BLOCK_BATTERY_VERSION   0x1

typedef enum 
{
    GAMEPAD_CMD_REFRESH, 
    GAMEPAD_CMD_SET_DEFAULT_MODE, 
    GAMEPAD_CMD_RESET_TO_BOOTLOADER, 
    GAMEPAD_CMD_ENABLE_BLUETOOTH_UPLOAD, 
    GAMEPAD_CMD_SAVE_ALL, 
} gamepad_cmd_t;

typedef enum 
{
    REMAP_CMD_REFRESH, 
} remap_cmd_t;

typedef enum 
{
    ANALOG_CMD_REFRESH, 
    ANALOG_CMD_CALIBRATE_START, 
    ANALOG_CMD_CALIBRATE_STOP, 
    ANALOG_CMD_CAPTURE_ANGLE, 
} analog_cmd_t;

typedef enum 
{
    RGB_CMD_REFRESH, 
} rgb_cmd_t;

typedef enum 
{
    IMU_CMD_REFRESH, 
    IMU_CMD_CALIBRATE_START, 
} imu_cmd_t;

typedef enum 
{
    HAPTIC_CMD_REFRESH, 
    HAPTIC_CMD_TEST_STRENGTH, 
} haptic_cmd_t;

typedef enum 
{
    TRIGGER_CMD_REFRESH, 
    TRIGGER_CMD_CALIBRATE_START, 
    TRIGGER_CMD_CALIBRATE_STOP, 
} trigger_cmd_t;

typedef void (*setting_callback_t)(const uint8_t *data, uint16_t size);
typedef void (*command_confirm_t)(cfg_block_t, uint8_t, uint8_t*, uint32_t);

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
    uint8_t     trigger_mode_gamecube; // Trigger mode for gamecube only
    uint16_t    left_min;
    uint16_t    left_max;
    uint16_t    left_deadzone : 15;
    uint16_t    left_disabled : 1;
    uint16_t    left_hairpin_value;
    uint16_t    left_static_output_value;
    uint16_t    right_min;
    uint16_t    right_max;
    uint16_t    right_deadzone : 15;
    uint16_t    right_disabled : 1;
    uint16_t    right_hairpin_value;
    uint16_t    right_static_output_value;
    uint8_t     reserved[42];
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
        uint8_t     analog_config_version; // 0
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
        uint16_t    l_deadzone; 
        uint16_t    r_deadzone; 
        uint8_t     reserved[365];
} analogConfig_s;

typedef struct 
{
    uint8_t     rgb_config_version;
    uint8_t     rgb_mode;
    uint8_t     rgb_speed;
    uint32_t    rgb_colors[32]; // Store 32 RGB colors
    uint16_t    rgb_brightness;
    uint8_t     reserved[123];
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

// Remapping struct used to determine
// remapping parameters
typedef struct
{
    uint8_t dpad_up     : 4; // Default 0
    uint8_t dpad_down   : 4; // Default 1
    uint8_t dpad_left   : 4; // Default 2
    uint8_t dpad_right  : 4; // Default 3...
    uint8_t button_a      : 4;
    uint8_t button_b      : 4;
    uint8_t button_x      : 4;
    uint8_t button_y      : 4;
    uint8_t trigger_l       : 4;
    uint8_t trigger_zl      : 4;
    uint8_t trigger_r       : 4;
    uint8_t trigger_zr      : 4;
    uint8_t button_plus   : 4;
    uint8_t button_minus  : 4;
    uint8_t button_stick_left     : 4;
    uint8_t button_stick_right    : 4;
} buttonRemap_s;

#define BUTTON_REMAP_SIZE sizeof(buttonRemap_s)

typedef struct
{
    uint8_t remap_config_version;
    // Switch, XInput, SNES, N64, GameCube
    buttonRemap_s profiles[12]; // SIZE=8
    uint16_t disabled[12]; // 12 disabled options (tells remap which buttons to disable)
    uint8_t  reserved[135];
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