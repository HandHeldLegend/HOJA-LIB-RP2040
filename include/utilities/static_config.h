#ifndef UTILITIES_STATIC_CONFIG_H
#define UTILITIES_STATIC_CONFIG_H

#include <stdint.h>
#include "settings_shared_types.h"

typedef enum 
{
    STATIC_BLOCK_DEVICE, // Half way through config block
    STATIC_BLOCK_BUTTONS, 
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
    uint8_t     fcc_id[32];
    uint16_t    fw_version; 
} deviceInfoStatic_s;

typedef struct 
{
    uint16_t    main_buttons;
    uint8_t     system_buttons;
} buttonInfoStatic_s;

typedef struct 
{
    uint8_t axis_lx : 1;
    uint8_t axis_ly : 1;
    uint8_t axis_rx : 1;
    uint8_t axis_ry : 1;
    uint8_t axis_lt : 1;
    uint8_t axis_rt : 1;
    uint8_t reserved : 2;
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
    uint16_t    capacity_mah;
    uint8_t     part_number[24];
} batteryInfoStatic_s;

typedef struct 
{
    uint8_t haptic_hd : 1;
    uint8_t haptic_sd : 1;
    uint8_t reserved  : 6;
} hapticInfoStatic_s;

typedef struct 
{
    uint8_t bluetooth_bdr : 1;
    uint8_t bluetooth_ble : 1;
    uint8_t reserved      : 6;
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
#define STATINFO_DEVICE_BUTTON_SIZE     sizeof(buttonInfoStatic_s)
#define STATINFO_ANALOG_SIZE            sizeof(analogInfoStatic_s)
#define STATINFO_IMU_SIZE               sizeof(imuInfoStatic_s)
#define STATINFO_BATTERY_SIZE           sizeof(batteryInfoStatic_s)
#define STATINFO_HAPTIC_SIZE            sizeof(hapticInfoStatic_s)
#define STATINFO_BLUETOOTH_SIZE         sizeof(bluetoothInfoStatic_s)
#define STATINFO_RGB_SIZE               sizeof(rgbInfoStatic_s)

extern const deviceInfoStatic_s     device_static; 
extern const buttonInfoStatic_s     buttons_static; 
extern const analogInfoStatic_s     analog_static; 
extern const imuInfoStatic_s        imu_static; 
extern const batteryInfoStatic_s    battery_static; 
extern const hapticInfoStatic_s     haptic_static;
extern const bluetoothInfoStatic_s  bluetooth_static;
extern rgbInfoStatic_s        rgb_static;

void static_config_read_block(static_block_t block, setting_callback_t cb);

#endif