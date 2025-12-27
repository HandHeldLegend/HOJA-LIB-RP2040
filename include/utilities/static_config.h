#ifndef UTILITIES_STATIC_CONFIG_H
#define UTILITIES_STATIC_CONFIG_H

#include <stdint.h>
#include "settings_shared_types.h"

#if __has_include("timestamp.h")
    #include "timestamp.h"
#endif

#ifndef BUILD_TIMESTAMP
    #define FIRMWARE_VERSION_TIMESTAMP 0x00000000
#else 
    #define FIRMWARE_VERSION_TIMESTAMP BUILD_TIMESTAMP
#endif

typedef enum 
{
    STATIC_BLOCK_DEVICE, // Half way through config block
    STATIC_BLOCK_INPUT, 
    STATIC_BLOCK_ANALOG, 
    STATIC_BLOCK_HAPTIC, 
    STATIC_BLOCK_IMU, 
    STATIC_BLOCK_BATTERY, 
    STATIC_BLOCK_BLUETOOTH, 
    STATIC_BLOCK_RGB, 
    STATIC_BLOCK_MAX, 
} static_block_t; // Compile-time device data that may need to be read out 

#pragma pack(push, 1)
typedef struct 
{
    uint8_t     name[16]; 
    uint8_t     maker[16];
    uint8_t     manifest_url[256];
    uint8_t     firmware_url[256];
    uint8_t     manual_url[128];
    uint8_t     reserved[32];
    uint32_t    fw_version; 
    uint8_t     snes_supported : 1;
    uint8_t     joybus_supported : 1;
    uint8_t     reserved_bits : 6;
    uint8_t     reserved_bytes[93];
} deviceInfoStatic_s;

typedef struct 
{
    uint8_t input_type; // 0=unused, 1=digital, 2=hover, 3=joystick
    uint8_t input_name[8]; // Char name of input
    uint8_t rgb_group; // Which RGB group is correlated with this input for reactive mode (Results are -1, 0 is unused)
} inputInfoSlot_s;

#define INPUTINFOSLOT_SIZE sizeof(inputInfoSlot_s)

typedef struct
{
    inputInfoSlot_s input_info[36]; // SIZE=10 
} inputInfoStatic_s;

typedef struct 
{
    uint8_t axis_lx : 1;
    uint8_t axis_ly : 1;
    uint8_t axis_rx : 1;
    uint8_t axis_ry : 1;
    uint8_t axis_lt : 1;
    uint8_t axis_rt : 1;
    uint8_t invert_allowed : 1;
    uint8_t reserved : 1;
} analogInfoStatic_s;

typedef struct 
{
    uint8_t axis_gyro_a : 1;
    uint8_t axis_gyro_b : 1;
    uint8_t axis_accel_a : 1;
    uint8_t axis_accel_b : 1;
    uint8_t reserved : 4;
} imuInfoStatic_s;

typedef struct 
{
    uint16_t    battery_capacity_mah;
    uint8_t     battery_part_number[24];
    uint8_t     pmic_status;
    uint8_t     pmic_part_number[24];
    uint8_t     fuelgauge_status;
    uint8_t     fuelgauge_part_number[24];
} batteryInfoStatic_s;

typedef struct 
{
    uint8_t haptic_hd : 1;
    uint8_t haptic_sd : 1;
    uint8_t reserved  : 6;
} hapticInfoStatic_s;

typedef struct 
{
    uint8_t     part_number[24];
    uint8_t     external_update_supported;
    uint16_t    external_version_number;
    uint8_t     bluetooth_bdr_supported;
    uint8_t     bluetooth_ble_supported;
    uint8_t     bluetooth_status;
    uint8_t     fcc_id[24];
} bluetoothInfoStatic_s;

typedef struct 
{
    uint8_t rgb_group_name[8];
} rgbGroupName_s;

typedef struct 
{
    uint8_t     rgb_groups; 
    rgbGroupName_s rgb_group_names[32]; // SIZE=8 
    int8_t      rgb_player_group;
} rgbInfoStatic_s;
#pragma pack(pop)

#define STATINFO_DEVICE_BLOCK_SIZE      sizeof(deviceInfoStatic_s)
#define STATINFO_DEVICE_INPUT_SIZE      sizeof(inputInfoStatic_s)
#define STATINFO_ANALOG_SIZE            sizeof(analogInfoStatic_s)
#define STATINFO_IMU_SIZE               sizeof(imuInfoStatic_s)
#define STATINFO_BATTERY_SIZE           sizeof(batteryInfoStatic_s)
#define STATINFO_HAPTIC_SIZE            sizeof(hapticInfoStatic_s)
#define STATINFO_BLUETOOTH_SIZE         sizeof(bluetoothInfoStatic_s)
#define STATINFO_RGB_SIZE               sizeof(rgbInfoStatic_s)

extern const deviceInfoStatic_s     device_static; 
extern analogInfoStatic_s           analog_static; 
extern const imuInfoStatic_s        imu_static; 
extern batteryInfoStatic_s          battery_static; 
extern const hapticInfoStatic_s     haptic_static;
extern const inputInfoStatic_s      input_static;
extern bluetoothInfoStatic_s        bluetooth_static;
extern rgbInfoStatic_s              rgb_static;

void static_config_init();
void static_config_read_block(static_block_t block, setting_callback_t cb);

#endif