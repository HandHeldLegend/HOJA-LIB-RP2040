#ifndef UTILITIES_STATIC_CONFIG_H
#define UTILITIES_STATIC_CONFIG_H

#include <stdint.h>

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

typedef union 
{
    struct 
    {
        uint8_t name[16]; 
        uint8_t maker[16];
        uint16_t fw_version; 
    };
    uint8_t device_info[34];
} device_static_u;

typedef union 
{
    struct 
    {
        uint16_t    main_buttons;
        uint8_t     system_buttons;
    };
    uint8_t buttons_info[3];
} buttons_static_u;

typedef union 
{
    struct 
    {
        uint8_t axis_lx : 1;
        uint8_t axis_ly : 1;
        uint8_t axis_rx : 1;
        uint8_t axis_ry : 1;
        uint8_t axis_lt : 1;
        uint8_t axis_rt : 1;
        uint8_t reserved : 2;
    };
    uint8_t available_axes;
} analog_static_u;

typedef union 
{
    struct 
    {
        uint8_t axis_gyro_a : 1;
        uint8_t axis_gyro_b : 1;
        uint8_t axis_accel_a : 1;
        uint8_t axis_accel_b : 1;
        uint8_t reserved : 4;
    };
    uint8_t available_axes;
} imu_static_u;

typedef union 
{
    struct 
    {
        uint16_t    capacity_mah;
        uint8_t     part_number[24];
    };
} battery_static_u;

typedef union 
{
    struct 
    {
        uint8_t haptic_hd : 1;
        uint8_t haptic_sd : 1;
        uint8_t reserved  : 6;
    };
    uint8_t available_haptics;
} haptic_static_u;

typedef union 
{
    struct 
    {
        uint8_t bluetooth_bdr : 1;
        uint8_t bluetooth_ble : 1;
        uint8_t reserved      : 6;
    };
    uint8_t available_bluetooth;
} bluetooth_static_u;

typedef union 
{
    struct 
    {
        uint8_t     rgb_groups; 
        uint8_t     rgb_group_names[32][8]; 
        int8_t      rgb_player_group;
    };
    uint8_t available_rgb[257];
} rgb_static_u;

extern const device_static_u    device_static; 
extern const buttons_static_u   buttons_static; 
extern const analog_static_u    analog_static; 
extern const imu_static_u       imu_static; 
extern const battery_static_u   battery_static; 
extern const haptic_static_u    haptic_static;
extern const bluetooth_static_u bluetooth_static;
extern const rgb_static_u       rgb_static;

#endif