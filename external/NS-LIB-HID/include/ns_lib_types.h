/**
 * @file ns_lib_types.h
 * @brief Shared data structures for NS-LIB-HID console-oriented state.
 *
 * Copyright (c) 2026 Mitchell Cairns, Hand Held Legend, LLC.
 *
 * Licensed under the Creative Commons Attribution-NonCommercial 4.0 International License
 * (CC BY-NC 4.0). Non-commercial use with attribution; commercial licensing from Hand Held Legend.
 * Full terms: https://creativecommons.org/licenses/by-nc/4.0/legalcode — see LICENSING.md in this folder
 *
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo and
 * related marks are trademarks of their respective owners.
 */

#ifndef NS_LIB_TYPES_H
#define NS_LIB_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    NS_DEVTYPE_UNDEFINED,
    NS_DEVTYPE_PROCON,
    NS_DEVTYPE_JOYCON_L,
    NS_DEVTYPE_JOYCON_R,
    NS_DEVTYPE_NES,
    NS_DEVTYPE_FAMI,
    NS_DEVTYPE_SNES_NA,
    NS_DEVTYPE_SNES_EU,
    NS_DEVTYPE_SNES_JP,
    NS_DEVTYPE_N64,
    NS_DEVTYPE_MEGADRIVE,
    NS_DEVTYPE_GENESIS
} ns_devtype_t;

typedef enum
{
    NS_TRANSPORT_UNDEFINED,
    NS_TRANSPORT_USB,
    NS_TRANSPORT_BTC,
    NS_TRANSPORT_UART, // Unsupported
} ns_transport_t;

typedef enum
{
    NS_IMU_OFF,
    NS_IMU_RAW,
    NS_IMU_QUAT,
} ns_imu_mode_t;

/** @brief RGB LED channels for body, grips, and buttons (host-reported or local mirror). */
typedef struct
{
    uint8_t body_r;
    uint8_t body_g;
    uint8_t body_b;
    uint8_t l_grip_r;
    uint8_t l_grip_g;
    uint8_t l_grip_b;
    uint8_t r_grip_r;
    uint8_t r_grip_g;
    uint8_t r_grip_b;
    uint8_t buttons_r;
    uint8_t buttons_g;
    uint8_t buttons_b;
} ns_colordata_s;

typedef struct
{
    ns_devtype_t type;
    ns_transport_t transport;
    /** Local controller MAC address used by protocol responses / pairing data. */
    uint8_t device_mac[6];
    /** Paired host MAC address (optional for protocol/pairing flows). */
    uint8_t host_mac[6];
    /** Body/grip/button colors used for SPI factory color pages. */
    ns_colordata_s colors;
    /**
     * Gyro full-scale setting: nominal ±range in °/s for the IMU (e.g. 2000 → ±2000 °/s on int16).
     * @ref ns_device_config_set derives @ref gyro_rad_per_lsb via @ref ns_motion_calculate_rps.
     */
    float gyro_full_scale_dps;
    /**
     * Radians per second per int16 gyro LSB. Written by @ref ns_device_config_set; use with
     * @ref ns_motion_update_quaternion.
     */
    float gyro_rad_per_lsb;
} ns_device_config_s;

/**
 * @brief Result of configuration validation or operations that depend on a configured device.
 */
typedef enum
{
    NS_CONFIG_OK = 0,
    /** Device identity / transport / output hook still default or incomplete. */
    NS_CONFIG_NOT_CONFIGURED,
    /** Bad pointer or inconsistent arguments. */
    NS_CONFIG_INVALID_ARG,
} ns_config_status_t;

/** @brief Stored pairing credentials for Bluetooth reconnect. */
typedef struct
{
    uint8_t host_mac[6]; /**< Paired console radio MAC, 6 bytes. */
    uint8_t link_key[16]; /**< Link key for encrypted ACL, 16 bytes. */
} ns_usbpair_s;

/**
 * @brief Battery / USB / charging status for Switch input report byte 1.
 *
 * Same wire layout as HOJA `bat_status_u` (`devices_shared_types.h`): bitfield order matches GCC
 * little-endian packing for `uint8_t val`.
 */
typedef union ns_powerstatus_s
{
    struct
    {
        uint8_t power_source : 1; /**< 0 = battery, 1 = external / USB power. */
        uint8_t connection : 2;   /**< Console-defined connection state (2 bits). */
        uint8_t reserved : 1;
        uint8_t charging : 1;
        uint8_t bat_lvl : 3; /**< Battery gauge level (0–7; HOJA fuel gauge uses 1–4 for quartiles). */
    };
    uint8_t val; /**< Packed byte written to input report @c [1]. */
} ns_powerstatus_s;

/** @brief Single IMU sample (gyroscope + accelerometer). */
typedef struct
{
    union
    {
        struct
        {
            uint8_t ax_8l : 8;
            uint8_t ax_8h : 8;
        };
        int16_t ax;
    };

    union
    {
        struct
        {
            uint8_t ay_8l : 8;
            uint8_t ay_8h : 8;
        };
        int16_t ay;
    };
    
    union
    {
        struct
        {
            uint8_t az_8l : 8;
            uint8_t az_8h : 8;
        };
        int16_t az;
    };
    
    union
    {
        struct
        {
            uint8_t gx_8l : 8;
            uint8_t gx_8h : 8;
        };
        int16_t gx;
    };

    union
    {
        struct
        {
            uint8_t gy_8l : 8;
            uint8_t gy_8h : 8;
        };
        int16_t gy;
    };
    
    union
    {
        struct
        {
            uint8_t gz_8l : 8;
            uint8_t gz_8h : 8;
        };
        int16_t gz;
    };

    uint64_t timestamp_us;
} ns_gyrodata_s;

/** Working quaternion + last accel snapshot mirrored into mode-2 packing (`raw` is x,y,z,w). */
typedef struct
{
    float raw[4];           /**< Components x, y, z, w (Hamilton / scalar-last indexing used by motion helpers). */
    int16_t ax;             /**< Last accelerometer sample copied from integration step (device units). */
    int16_t ay;
    int16_t az;
    uint64_t timestamp_us; /**< Monotonic sample time in microseconds (same domain as @ref ns_motion_imu_sample_s). */
} ns_quaternion_s;


/** @brief Byte-level 0x30 report input payload (buttons + packed sticks). */
typedef struct
{
    union
    {
        struct
        {
            // Y and C-Up (N64)
            uint8_t b_y       : 1;

            // X and C-Left (N64)
            uint8_t b_x       : 1;

            uint8_t b_b       : 1;
            uint8_t b_a       : 1;
            uint8_t t_r_sr    : 1;
            uint8_t t_r_sl    : 1;
            uint8_t t_r       : 1;

            // ZR and C-Down (N64)
            uint8_t t_zr      : 1;
        };
        uint8_t right_buttons;
    };
    union
    {
        struct
        {
            // Minus and C-Right (N64)
            uint8_t b_minus     : 1;

            // Plus and Start
            uint8_t b_plus      : 1;

            uint8_t sb_right    : 1;
            uint8_t sb_left     : 1;
            uint8_t b_home      : 1;
            uint8_t b_capture   : 1;
            uint8_t none        : 1;
            uint8_t charge_grip_active : 1;
        };
        uint8_t shared_buttons;
    };
    union
    {
        struct
        {
            uint8_t d_down    : 1;
            uint8_t d_up      : 1;
            uint8_t d_right   : 1;
            uint8_t d_left    : 1;
            uint8_t t_l_sr    : 1;
            uint8_t t_l_sl    : 1;
            uint8_t t_l       : 1;

            // ZL and Z (N64)
            uint8_t t_zl      : 1;

        };
        uint8_t left_buttons;
    };
    uint8_t left_stick[3];
    uint8_t right_stick[3];
} ns_inputdata_s;

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_TYPES_H */
