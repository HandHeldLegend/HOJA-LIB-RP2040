/**
 * @file ns_lib_motion.h
 * @brief IMU mode-2 quaternion packing definitions.
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

#ifndef NS_LIB_MOTION_H
#define NS_LIB_MOTION_H

#include <stddef.h>
#include <stdint.h>

#include "ns_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Precompute radians per second per int16 gyro LSB from the IMU ±full-scale (°/s).
 *
 * Assumes a symmetric 16-bit gyro: ±32768 counts ↔ ±@p full_scale_dps °/s. Then one LSB is
 * `full_scale_dps / 32768` °/s; multiply by π/180 to obtain rad/s per LSB for
 * @ref ns_motion_update_quaternion.
 *
 * @param full_scale_dps Nominal half-range in degrees/s (e.g. 2000 for ±2000 °/s).
 * @return rad/s per int16 LSB.
 */
float ns_motion_calculate_rps(float full_scale_dps);

/** Integration context: stores previous sample time for delta-t in @ref ns_motion_update_quaternion. */
typedef struct
{
    uint64_t prev_timestamp_us; /**< Zero until first sample; then prior `sample->timestamp_us`. */
} ns_motion_quat_integrator_s;

/** Compact int16 vector used inside @ref ns_mode_2_s accel slots. */
typedef struct
{
    int16_t y;
    int16_t x;
    int16_t z;
} ns_motion_vector_s;

/** Packed Switch IMU “mode 2” blob (bitfields; `#pragma pack` layout matches console expectations). */
#pragma pack(push, 1)
typedef struct
{
    ns_motion_vector_s accel_0;
    uint32_t mode : 2;
    uint32_t max_index : 2;
    uint32_t last_sample_0 : 21;
    uint32_t last_sample_1l : 7;
    uint16_t last_sample_1h : 14;
    uint16_t last_sample_2l : 2;
    ns_motion_vector_s accel_1;
    uint32_t last_sample_2h : 19;
    uint32_t delta_last_first_0 : 13;
    uint16_t delta_last_first_1 : 13;
    uint16_t delta_last_first_2l : 3;
    ns_motion_vector_s accel_2;
    uint32_t delta_last_first_2h : 10;
    uint32_t delta_mid_avg_0 : 7;
    uint32_t delta_mid_avg_1 : 7;
    uint32_t delta_mid_avg_2 : 7;
    uint32_t timestamp_start_l : 1;
    uint16_t timestamp_start_h : 10;
    uint16_t timestamp_count : 6;
} ns_mode_2_s;
#pragma pack(pop)

/** Byte length of @ref ns_mode_2_s for reports / memcpy sizing. */
#define NS_MODE2_DATA_SIZE sizeof(ns_mode_2_s)

/**
 * @brief Reset quaternion state and integration timing.
 *
 * Sets `state` to identity (0,0,0,1), clears mirrored accel and timestamp fields, and clears
 * `integrator->prev_timestamp_us` so the next @ref ns_motion_update_quaternion call does not
 * apply a bogus delta-t.
 *
 * @param state       Quaternion workspace; may be NULL (then only integrator is touched).
 * @param integrator  Integration context; may be NULL (then only state is touched).
 */
void ns_motion_quaternion_reset(ns_quaternion_s *state, ns_motion_quat_integrator_s *integrator);

/**
 * @brief Helper: integrate one gyro sample into a quaternion (incremental step).
 *
 * Convenience implementation for firmware that does **not** run its own fusion loop. It computes `dt`
 * from successive `sample->timestamp_us` values (skipped on first sample or when time goes backward),
 * then applies incremental rotation using @p gyro_rad_per_lsb (rad/s per int16 LSB from
 * @ref ns_motion_calculate_rps or @ref ns_device_config_s::gyro_rad_per_lsb).
 *
 * @note **Preferred path when IMU mode 2 is enabled:** fuse on every IMU read in your loop, keep
 * `ns_quaternion_s` locally, and in @ref ns_get_imu_quaternion_cb copy the latest state into @p out.
 *
 * @note The default weak @ref ns_get_imu_quaternion_cb integrates **once per callback** only.
 *
 * @param[in,out] state            Current quaternion; must be non-NULL.
 * @param[in,out] integrator       Holds previous timestamp; must be non-NULL.
 * @param[in] sample               Gyro + accel + monotonic `timestamp_us`; must be non-NULL.
 * @param[in] gyro_rad_per_lsb     rad/s per int16 gyro count (same for all axes on a typical IMU).
 */
void ns_motion_update_quaternion(ns_quaternion_s *state, ns_motion_quat_integrator_s *integrator,
                                 const ns_gyrodata_s *sample, float gyro_rad_per_lsb);

/**
 * @brief Pack @p in into Switch IMU mode-2 wire layout (@p out).
 *
 * Chooses the largest magnitude quaternion component as reference, encodes the other three into the
 * packed bitfields, fills accel slot 0 from `in->ax/ay/az`, sets mode
 * and timestamp fields from `in->timestamp_us` (converted to milliseconds for the 11-bit wire counter).
 * Delta/average fields are zeroed in the current
 * implementation.
 *
 * @param[in]  in  Normalized quaternion + accel/timestamp from integration.
 * @param[out] out Packed structure for copying into the input report IMU region.
 */
void ns_motion_pack_quat(ns_quaternion_s *in, ns_mode_2_s *out);

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_MOTION_H */
