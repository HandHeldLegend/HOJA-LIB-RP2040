/**
 * @file ns_lib_motion.c
 * @brief Mode-2 packed quaternion report builder (NS-LIB-HID).
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

#include "ns_lib_motion.h"

#include <math.h>


#define NS_LIB_MOTION_PI_F 3.14159265358979323846f
#define NS_LIB_MOTION_EPS_F 1e-12f

static void _ns_motion_quat_mul_inplace(ns_quaternion_s *a, const ns_quaternion_s *b)
{
    float w = a->raw[3] * b->raw[3] - a->raw[0] * b->raw[0] - a->raw[1] * b->raw[1] - a->raw[2] * b->raw[2];
    float x = a->raw[3] * b->raw[0] + a->raw[0] * b->raw[3] + a->raw[1] * b->raw[2] - a->raw[2] * b->raw[1];
    float y = a->raw[3] * b->raw[1] - a->raw[0] * b->raw[2] + a->raw[1] * b->raw[3] + a->raw[2] * b->raw[0];
    float z = a->raw[3] * b->raw[2] + a->raw[0] * b->raw[1] - a->raw[1] * b->raw[0] + a->raw[2] * b->raw[3];

    a->raw[0] = x;
    a->raw[1] = y;
    a->raw[2] = z;
    a->raw[3] = w;
}

static void _ns_motion_quat_normalize(ns_quaternion_s *q)
{
    float norm_sq = q->raw[0] * q->raw[0] + q->raw[1] * q->raw[1] + q->raw[2] * q->raw[2] + q->raw[3] * q->raw[3];
    if (norm_sq <= NS_LIB_MOTION_EPS_F)
    {
        q->raw[0] = 0.0f;
        q->raw[1] = 0.0f;
        q->raw[2] = 0.0f;
        q->raw[3] = 1.0f;
        return;
    }
    float inv_norm = 1.0f / sqrtf(norm_sq);
    q->raw[0] *= inv_norm;
    q->raw[1] *= inv_norm;
    q->raw[2] *= inv_norm;
    q->raw[3] *= inv_norm;
}

void ns_motion_quaternion_reset(ns_quaternion_s *state, ns_motion_quat_integrator_s *integrator)
{
    if (state != NULL)
    {
        state->raw[0] = 0.0f;
        state->raw[1] = 0.0f;
        state->raw[2] = 0.0f;
        state->raw[3] = 1.0f;
        state->ax = 0;
        state->ay = 0;
        state->az = 0;
        state->timestamp_us = 0;
    }
    if (integrator != NULL)
    {
        integrator->prev_timestamp_us = 0;
    }
}

float ns_motion_calculate_rps(float full_scale_dps)
{
    return (full_scale_dps / 32768.0f) * (NS_LIB_MOTION_PI_F / 180.0f);
}

void ns_motion_update_quaternion(ns_quaternion_s *state, ns_motion_quat_integrator_s *integrator,
                                 const ns_gyrodata_s *sample, float gyro_rad_per_lsb)
{
    if (state == NULL || integrator == NULL || sample == NULL)
    {
        return;
    }

    float dt = 0.0f;
    if (integrator->prev_timestamp_us != 0 && sample->timestamp_us > integrator->prev_timestamp_us)
    {
        dt = (float)(sample->timestamp_us - integrator->prev_timestamp_us) / 1000000.0f;
    }
    integrator->prev_timestamp_us = sample->timestamp_us;

    float scale_dt = gyro_rad_per_lsb * dt;

    /*
     * Coordinate mapping kept from existing HOJA IMU path:
     *   quat x-axis integrates sensor gy
     *   quat y-axis integrates sensor gx
     *   quat z-axis integrates sensor gz
     */
    float angle_x = (float)sample->gy * scale_dt;
    float angle_y = (float)sample->gx * scale_dt;
    float angle_z = (float)sample->gz * scale_dt;

    /*
     * Delta quaternion from rotation vector.
     * Using sin/cos gives better precision than the old 4th-order Taylor approximation. (Thanks banz!)
     */
    float norm = sqrtf(angle_x * angle_x + angle_y * angle_y + angle_z * angle_z);
    float half = 0.5f * norm;
    float vec_scale;
    float scalar;
    if (norm > NS_LIB_MOTION_EPS_F)
    {
        vec_scale = sinf(half) / norm;
        scalar = cosf(half);
    }
    else
    {
        vec_scale = 0.5f;
        scalar = 1.0f;
    }

    ns_quaternion_s delta = {
        .raw = {
            angle_x * vec_scale,
            angle_y * vec_scale,
            angle_z * vec_scale,
            scalar,
        },
    };

    _ns_motion_quat_mul_inplace(state, &delta);
    _ns_motion_quat_normalize(state);

    state->ax = sample->ax;
    state->ay = sample->ay;
    state->az = sample->az;
    state->timestamp_us = sample->timestamp_us;
}

void ns_motion_pack_quat(ns_quaternion_s *in, ns_mode_2_s *out)
{
    out->mode = 2;

    uint8_t max_index = 0;
    for (uint8_t i = 1; i < 4; i++)
    {
        if (fabsf(in->raw[i]) > fabsf(in->raw[max_index]))
        {
            max_index = i;
        }
    }

    out->max_index = max_index;

    int quaternion_30bit_components[3];

    for (int i = 0; i < 3; ++i)
    {
        quaternion_30bit_components[i] =
            (int)(in->raw[(max_index + i + 1) & 3] * 0x40000000 * (in->raw[max_index] < 0 ? -1 : 1));
    }

    out->last_sample_0 = (uint32_t)(quaternion_30bit_components[0] >> 10);

    out->last_sample_1l = (uint32_t)(((quaternion_30bit_components[1] >> 10) & 0x7F));
    out->last_sample_1h = (uint16_t)((((quaternion_30bit_components[1] >> 10) & 0x1FFF80) >> 7));

    out->last_sample_2l = (uint16_t)(((quaternion_30bit_components[2] >> 10) & 0x3));
    out->last_sample_2h = (uint32_t)((((quaternion_30bit_components[2] >> 10) & 0x1FFFFC) >> 2));

    out->delta_last_first_0 = 0;
    out->delta_last_first_1 = 0;
    out->delta_last_first_2l = 0;
    out->delta_last_first_2h = 0;
    out->delta_mid_avg_0 = 0;
    out->delta_mid_avg_1 = 0;
    out->delta_mid_avg_2 = 0;

    /* Wire format matches legacy switch_motion: 11-bit counter in milliseconds. */
    uint64_t time_ms = in->timestamp_us / 1000u;
    out->timestamp_start_l = (uint32_t)(time_ms & 0x1u);
    out->timestamp_start_h = (uint16_t)((time_ms >> 1) & 0x3FFu);
    out->timestamp_count = 3;

    out->accel_0.x = in->ax;
    out->accel_0.y = in->ay;
    out->accel_0.z = in->az;
}
