#ifndef SETTINGS_SHARED_TYPES_H
#define SETTINGS_SHARED_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum 
{
    CFG_BLOCK_GAMEPAD, 
    CFG_BLOCK_HOVER, 
    CFG_BLOCK_ANALOG, 
    CFG_BLOCK_RGB, 
    CFG_BLOCK_TRIGGER, 
    CFG_BLOCK_IMU, 
    CFG_BLOCK_HAPTIC, 
    CFG_BLOCK_USER, 
    CFG_BLOCK_INPUT,
    CFG_BLOCK_MAX,
} cfg_block_t;

#define CFG_BLOCK_GAMEPAD_VERSION   0x12

// Remap config is replaced by hover cfg
//#define CFG_BLOCK_REMAP_VERSION     0x13
#define CFG_BLOCK_HOVER_VERSION     0x14

#define CFG_BLOCK_ANALOG_VERSION    0x11
#define CFG_BLOCK_RGB_VERSION       0x11

// Reserved for later use now
#define CFG_BLOCK_TRIGGER_VERSION   0x11

#define CFG_BLOCK_IMU_VERSION       0x11
#define CFG_BLOCK_HAPTIC_VERSION    0x11
#define CFG_BLOCK_USER_VERSION      0x11

// Battery cfg is removed
//#define CFG_BLOCK_BATTERY_VERSION   0x12
#define CFG_BLOCK_INPUT_VERSION     0x13

typedef enum 
{
    GAMEPAD_CMD_REFRESH, 
    GAMEPAD_CMD_RESET_TO_BOOTLOADER, 
    GAMEPAD_CMD_ENABLE_BLUETOOTH_UPLOAD, 
    GAMEPAD_CMD_SAVE_ALL = 0xFF, 
} gamepad_cmd_t;

typedef enum 
{
    MAPPER_CMD_REFRESH, 
    MAPPER_CMD_DEFAULT_ALL,
    MAPPER_CMD_DEFAULT_SWITCH,
    MAPPER_CMD_DEFAULT_XINPUT,
    MAPPER_CMD_DEFAULT_SNES,
    MAPPER_CMD_DEFAULT_N64,
    MAPPER_CMD_DEFAULT_GAMECUBE,
    MAPPER_CMD_DEFAULT_SINPUT,
    MAPPER_CMD_WEBUSB_SWITCH,
    MAPPER_CMD_WEBUSB_XINPUT,
    MAPPER_CMD_WEBUSB_SNES,
    MAPPER_CMD_WEBUSB_N64,
    MAPPER_CMD_WEBUSB_GAMECUBE,
    MAPPER_CMD_WEBUSB_SINPUT,
} mapper_cmd_t;

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

typedef void (*setting_callback_t)(const uint8_t *data, uint16_t size);
typedef void (*command_confirm_t)(cfg_block_t, uint8_t, uint8_t*, uint32_t);
typedef void (*webreport_cmd_confirm_t)(
    uint8_t block, uint8_t command, bool success, 
    uint8_t* data, uint32_t size);

#pragma pack(push, 1)

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

// Trigger config is now unused
// We can safely use this later for other
// data
typedef struct 
{
    uint8_t reserved[64];
} triggerConfig_s;

typedef struct 
{
    uint8_t dpad_config_version;
    uint8_t socd_type;
    uint16_t dpad_deadzone;
} dpadConfig_s;

typedef struct 
{
    float in_angle;
    float out_angle;
    float deadzone;
    float in_distance;
    float out_distance;
    bool enabled;
} joyConfigSlot_s;

#define JOY_CFG_SIZE sizeof(joyConfigSlot_s)

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
        joyConfigSlot_s  joy_config_l[16]; // SIZE=21
        joyConfigSlot_s  joy_config_r[16]; // SIZE=21
        uint16_t    l_deadzone; 
        uint16_t    r_deadzone; 
        uint8_t     l_snapback_type;
        uint8_t     r_snapback_type;
        uint16_t    l_deadzone_outer; 
        uint16_t    r_deadzone_outer; 
        uint16_t    l_snapback_intensity; 
        uint16_t    r_snapback_intensity;
        uint16_t    l_threshold; // Analog->Digital threshold for left joystick
        uint16_t    r_threshold; // Analog->Digital threshold for right joystick
        uint8_t     reserved[325];
} analogConfig_s; // 1024 bytes

#define ACFGSIZE sizeof(analogConfig_s)

#define RGB_BRIGHTNESS_MAX 4096

typedef struct 
{
    uint8_t     rgb_config_version;
    uint8_t     rgb_mode;
    uint16_t    rgb_speed; // RGB Speed in ms
    uint32_t    rgb_colors[32]; // Store 32 RGB colors
    uint16_t    rgb_brightness; // 4096 range
    uint8_t     reserved[122];
} rgbConfig_s;

typedef struct 
{
    uint8_t  gamepad_config_version;
    uint8_t  gamepad_default_mode;
    uint8_t  switch_mac_address[6]; // Mac address used to connect to Switch (BASE DEVICE MAC)
    uint32_t gamepad_color_body;
    uint32_t gamepad_color_buttons;
    uint32_t gamepad_color_grip_left;
    uint32_t gamepad_color_grip_right;
    uint8_t  host_mac_switch[6]; // Mac address of the Switch we are paired to
    uint8_t  host_mac_sinput[6]; // Mac address of the SInput device we are paired to
    uint8_t  webusb_enable_popup; // Whether or not the WebUSB toast should show
    uint8_t  reserved[27];
} gamepadConfig_s;

// Calibration data used for analog inputs (non-joystick)
typedef struct 
{
    uint16_t invert : 1;
    uint16_t min : 15;
    uint16_t max;
} hoverSlot_s;

#define HOVER_SLOT_SIZE sizeof(hover_cfg_s) 

typedef struct 
{
    uint8_t hover_config_version;
    uint8_t hover_calibration_set;
    hoverSlot_s config[36]; // SIZE=4
    uint8_t reserved[111];
} hoverConfig_s;

// Input config replaces our remap config
// It also now contains information for when it will be used
// for analog to digital or digital to analog inputs
typedef struct
{
    uint16_t output_mode : 3; // 0=default, 1=rapid trigger, 2=threshold
    uint16_t static_output : 13; // Output that is used when this input is pressed
    uint16_t threshold_delta; // Either a threshold for digital press or a delta for rapid trigger
    int8_t output_code; // Code for what this outputs or is assigned to
} inputConfigSlot_s;

#define INPUT_SLOT_SIZE sizeof(input_cfg_s) 

typedef struct 
{
    uint8_t input_config_version;
    inputConfigSlot_s input_profile_switch[36]; // SIZE=5
    inputConfigSlot_s input_profile_xinput[36]; // SIZE=5
    inputConfigSlot_s input_profile_snes[36]; // SIZE=5
    inputConfigSlot_s input_profile_n64[36]; // SIZE=5
    inputConfigSlot_s input_profile_gamecube[36]; // SIZE=5
    inputConfigSlot_s input_profile_sinput[36]; // SIZE=5
    inputConfigSlot_s input_profile_reserved_2[36]; // SIZE=5
    uint8_t reserved[787];
} inputConfig_s;

#pragma pack(pop)

// Byte sizes of our various blocks
#define GAMEPAD_CFB_SIZE    sizeof(gamepadConfig_s) // 64
// Remap config is depreciated and replace this block with hover Config
// #define REMAP_CFB_SIZE      sizeof(remapConfig_s) // 256
#define HOVER_CFB_SIZE      sizeof(hoverConfig_s) // 256

#define RGB_CFB_SIZE        sizeof(rgbConfig_s) // 256
#define ANALOG_CFB_SIZE     sizeof(analogConfig_s) // 1024

// Trigger config is replaced and space is simply reserved
#define TRIGGER_CFB_SIZE    sizeof(triggerConfig_s) // 64

#define IMU_CFB_SIZE        sizeof(imuConfig_s) // 32
#define HAPTIC_CFB_SIZE     sizeof(hapticConfig_s) 
#define USER_CFB_SIZE       sizeof(userConfig_s) // 64
// Battery cfb was never used. Replace with more useful data
// #define BATTERY_CFB_SIZE    sizeof(batteryConfig_s) // 16
#define INPUT_CFB_SIZE      sizeof(inputConfig_s) // 2048

// Byte size of all combined blocks
#define TOTAL_CFB_SIZE (GAMEPAD_CFB_SIZE+HOVER_CFB_SIZE+RGB_CFB_SIZE+\
                        ANALOG_CFB_SIZE+TRIGGER_CFB_SIZE+IMU_CFB_SIZE+HAPTIC_CFB_SIZE+\
                        USER_CFB_SIZE+INPUT_CFB_SIZE)

                        

#endif