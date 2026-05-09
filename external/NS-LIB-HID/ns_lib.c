/**
 * @file ns_lib.c
 * @brief Weak default implementations of NS-LIB-HID console callback hooks.
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

#include "ns_lib.h"
#include "ns_lib_protocol.h"

__attribute__((weak)) void ns_set_haptic_indices_cb(const ns_lib_haptic_raw_sample_s *pairs, uint8_t pair_count)
{
    (void)pairs;
    (void)pair_count;
}

__attribute__((weak)) void ns_set_led_cb(int player_leds)
{
    (void)player_leds;
}

__attribute__((weak)) void ns_set_power_cb(uint8_t shutdown)
{
    (void)shutdown;
}

__attribute__((weak)) void ns_set_usbpair_cb(ns_usbpair_s pairing_data)
{
    (void)pairing_data;
}

__attribute__((weak)) void ns_set_imumode_cb(ns_imu_mode_t imu_mode)
{
    (void)imu_mode;
}


__attribute__((weak)) void ns_get_powerstatus_cb(ns_powerstatus_s *out)
{
    if (!out)
    {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->bat_lvl = 4;
    out->power_source = 1;
    out->connection = 1;
    out->charging = 0;
    out->reserved = 0;
}

__attribute__((weak)) void ns_get_imu_raw_cb(ns_gyrodata_s *out)
{
    if (!out)
    {
        return;
    }
    memset(out, 0, sizeof(*out));
}

__attribute__((weak)) void ns_get_inputdata_cb(ns_inputdata_s *out)
{
    if (!out)
    {
        return;
    }
    memset(out, 0, sizeof(*out));
    ns_analog_pack_xy12(2048u, 2048u, out->left_stick);
    ns_analog_pack_xy12(2048u, 2048u, out->right_stick);
}

__attribute__((weak)) void ns_linkkey_get_cb(uint8_t *out)
{
    if (!out)
    {
        return;
    }
    memset(out, 0, 16);
}

__attribute__((weak)) void ns_devmac_get_cb(uint8_t *out)
{
    if (!out)
    {
        return;
    }
    memset(out, 0, 6);
}

__attribute__((weak)) void ns_hostmac_get_cb(uint8_t *out)
{
    if (!out)
    {
        return;
    }
    memset(out, 0, 6);
}

__attribute__((weak)) void ns_get_imu_quaternion_cb(ns_quaternion_s *out)
{
    if (!out)
    {
        return;
    }
    static ns_quaternion_s quat_state = {.raw = {0.f, 0.f, 0.f, 1.f}};
    static ns_motion_quat_integrator_s integrator = {0};

    ns_gyrodata_s g = {0};
    ns_get_imu_raw_cb(&g);

    ns_motion_imu_sample_s sample = {
        .gx = g.gyro_x,
        .gy = g.gyro_y,
        .gz = g.gyro_z,
        .ax = g.accel_x,
        .ay = g.accel_y,
        .az = g.accel_z,
        .timestamp_us = g.timestamp_us,
    };

    ns_device_config_s cfg = {0};
    ns_device_config_get(&cfg);

    ns_motion_update_quaternion(&quat_state, &integrator, &sample, cfg.gyro_rad_per_lsb);
    *out = quat_state;
}

static void ns_pack_i16_le(uint8_t *dst, int16_t v)
{
    dst[0] = (uint8_t)((unsigned)v & 0xFFu);
    dst[1] = (uint8_t)(((unsigned)v >> 8) & 0xFFu);
}

/** Fills one 12-byte IMU group: ay, ax, az, gy, gx, gz as int16 LE (matches legacy swcmd layout). */
static void ns_lib_fill_imu_group12(const ns_gyrodata_s *g, uint8_t *dst12)
{
    ns_pack_i16_le(dst12 + 0, g->accel_y);
    ns_pack_i16_le(dst12 + 2, g->accel_x);
    ns_pack_i16_le(dst12 + 4, g->accel_z);
    ns_pack_i16_le(dst12 + 6, g->gyro_y);
    ns_pack_i16_le(dst12 + 8, g->gyro_x);
    ns_pack_i16_le(dst12 + 10, g->gyro_z);
}

static void ns_lib_pack_imu_standard_groups36(uint8_t *report)
{
    ns_gyrodata_s g = {0};
    ns_get_imu_raw_cb(&g);
    ns_lib_fill_imu_group12(&g, report + 12);
    memcpy(report + 24, report + 12, 12);
    memcpy(report + 36, report + 12, 12);
}

ns_config_status_t ns_api_init(const ns_device_config_s *cfg)
{
    ns_config_status_t st = ns_device_config_set(cfg);
    if (st != NS_CONFIG_OK)
    {
        return st;
    }
    ns_analog_calibration_init();
    ns_lib_haptics_init();
    return NS_CONFIG_OK;
}

void ns_api_generate_inputreport(uint8_t *data, uint16_t len)
{
    if (data == NULL || len < 2u)
    {
        return;
    }
    ns_lib_protocol_generate_inputreport(&data[0], &data[1]);
}

ns_config_status_t ns_lib_init(const ns_device_config_s *cfg)
{
    return ns_api_init(cfg);
}

void ns_api_output_tunnel(uint8_t *data, uint16_t len)
{
    (void)ns_lib_protocol_enqueue_host_input(data, len);
}

/*
 * Platform hooks (declared in ns_lib_api.h): weak definitions — firmware should supply strong replacements.
 */
__attribute__((weak)) void ns_get_time_ms(uint64_t *ms)
{
    static uint64_t t;
    if (!ms)
    {
        return;
    }
    t++;
    *ms = t;
}

__attribute__((weak)) uint8_t ns_get_random_u8(void)
{
    static uint16_t s = 0xACE1u;
    s = (uint16_t)(s * 1103515245u + 12345u);
    return (uint8_t)(s >> 8);
}

__attribute__((weak)) void ns_fill_imu_standard_report(uint8_t *report)
{
    if (!report)
    {
        return;
    }
    ns_lib_pack_imu_standard_groups36(report);
}
