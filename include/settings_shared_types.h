#ifndef SETTINGS_SHARED_TYPES_H
#define SETTINGS_SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

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

#define CFG_BLOCK_GAMEPAD_VERSION   0x12
#define CFG_BLOCK_REMAP_VERSION     0x12 
#define CFG_BLOCK_ANALOG_VERSION    0x11
#define CFG_BLOCK_RGB_VERSION       0x11
#define CFG_BLOCK_TRIGGER_VERSION   0x11
#define CFG_BLOCK_IMU_VERSION       0x11
#define CFG_BLOCK_HAPTIC_VERSION    0x11
#define CFG_BLOCK_USER_VERSION      0x11
#define CFG_BLOCK_BATTERY_VERSION   0x12

typedef enum 
{
    GAMEPAD_CMD_REFRESH, 
    GAMEPAD_CMD_RESET_TO_BOOTLOADER, 
    GAMEPAD_CMD_ENABLE_BLUETOOTH_UPLOAD, 
    GAMEPAD_CMD_SAVE_ALL = 0xFF, 
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
typedef void (*webreport_cmd_confirm_t)(
    uint8_t block, uint8_t command, bool success, 
    uint8_t* data, uint32_t size);

#pragma pack(push, 1)
typedef struct 
{
    uint8_t battery_config_version;
    float   charge_level_percent;
    uint8_t reserved[11];
} batteryConfig_s;

typedef struct 
{
    uint8_t user_config_version;
    uint8_t user_name[24];
    uint8_t reserved[39];
} userConfig_s;

typedef struct 
{
    uint8_t     haptic_config_version;
    uint8_t     haptic_strength;
    uint8_t     haptic_triggers;
    uint8_t     reserved[5];
} hapticConfig_s;

typedef struct 
{
    uint8_t     imu_config_version;
    int8_t      imu_a_gyro_offsets[3];
    int8_t      imu_a_accel_config[3];
    int8_t      imu_b_gyro_offsets[3];
    int8_t      imu_b_accel_config[3];
    uint8_t     imu_disabled;
    uint8_t     reserved[18];
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
        uint8_t     l_snapback_type;
        uint8_t     r_snapback_type;
        uint8_t     reserved[363];
} analogConfig_s;

typedef struct 
{
    uint8_t     rgb_config_version;
    uint8_t     rgb_mode;
    uint16_t    rgb_speed; // RGB Speed in ms
    uint32_t    rgb_colors[32]; // Store 32 RGB colors
    uint16_t    rgb_brightness;
    uint8_t     reserved[122];
} rgbConfig_s;

typedef struct 
{
    uint8_t  gamepad_config_version;
    uint8_t  gamepad_default_mode;
    uint8_t  switch_mac_address[6];
    uint32_t gamepad_color_body;
    uint32_t gamepad_color_buttons;
    uint32_t gamepad_color_grip_left;
    uint32_t gamepad_color_grip_right;
    uint8_t  reserved[40];
} gamepadConfig_s;

// Remapping struct used to determine
// remapping parameters
typedef struct
{
    int8_t dpad_up; // Default 0
    int8_t dpad_down; // Default 1
    int8_t dpad_left; // Default 2
    int8_t dpad_right; // Default 3...
    int8_t button_a;
    int8_t button_b;
    int8_t button_x;
    int8_t button_y;
    int8_t trigger_l;
    int8_t trigger_zl;
    int8_t trigger_r;
    int8_t trigger_zr;
    int8_t button_plus;
    int8_t button_minus;
    int8_t button_stick_left;
    int8_t button_stick_right;
} buttonRemap_s;

#define BUTTON_REMAP_SIZE sizeof(buttonRemap_s)

typedef struct
{
    uint8_t remap_config_version;
    // Switch, XInput, SNES, N64, GameCube
    buttonRemap_s profiles[12]; // SIZE=16
    uint8_t  reserved[63];
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