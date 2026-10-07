/**
 * @file nwii_lib_aim.h
 * @brief Motion aiming: turns a gamepad's gyro and accelerometer into a Wii Remote pointer
 *        position and roll, for controllers that have motion sensors but no IR camera.
 *
 * The helper fuses both sensors into a gravity estimate, then measures aim in world space:
 * left/right is rotation about the real vertical, up/down is rotation about the horizontal axis
 * across the pointing direction. That makes aiming behave the same whether the gamepad is held
 * flat, stood up facing the screen or anywhere between, and keeps a rolled grip from turning
 * horizontal motion into diagonal motion. The gyro's resting offset is learned whenever the
 * controller is held still.
 *
 * Controller frame used for every sensor input (right-handed):
 *   +X toward the controller's left side
 *   +Y toward the player (the back edge, held flat)
 *   +Z up, out of the controller's face
 * Gyro rates are positive for right-handed rotation about those axes. If your IMU is mounted
 * differently, remap its axes into this frame before calling nwii_aim_update().
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef NWII_LIB_AIM_H
#define NWII_LIB_AIM_H

#include <stdint.h>
#include <stdbool.h>

#include "nwii_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Tuning. nwii_aim_default_config() fills tested values. */
typedef struct
{
    float yaw_range_deg;   ///< Rotation from the centre to the left/right screen edge
    float pitch_range_deg; ///< Rotation from the centre to the top/bottom screen edge
    float deadband_dps;    ///< Rotation slower than this (after bias removal) is ignored
    float still_dps;       ///< Every axis under this counts as held still...
    float still_s;         ///< ...once it has lasted this long; the gyro offset is then learned
    float bias_tau_s;      ///< Time constant for learning the gyro offset while still
    float gravity_tau_s;   ///< How quickly the gravity estimate follows the accelerometer
} nwii_aim_config_s;

/** @brief Aim state. x, y and roll_rad are the outputs; treat the rest as private. */
typedef struct
{
    float x;        ///< Cursor x, -1.0 (left edge) .. +1.0 (right edge)
    float y;        ///< Cursor y, -1.0 (bottom edge) .. +1.0 (top edge)
    float roll_rad; ///< Roll about the pointing direction, positive turning clockwise from behind

    nwii_aim_config_s cfg;
    float up[3];    ///< Unit vector pointing up, in the controller frame
    bool  up_valid;
    float bias_dps[3];
    float still_s;
} nwii_aim_s;

/**
 * @brief Fill a config with defaults: +/-12.5 deg yaw and +/-11.5 deg pitch to the screen edges.
 */
void nwii_aim_default_config(nwii_aim_config_s *cfg);

/**
 * @brief Reset the aim (centred, gravity unknown, gyro offset zero).
 *
 * @param aim State to initialize.
 * @param cfg Tuning; NULL selects nwii_aim_default_config().
 */
void nwii_aim_init(nwii_aim_s *aim, const nwii_aim_config_s *cfg);

/**
 * @brief Feed one sensor sample.
 *
 * @param aim State.
 * @param gyro_dps Angular rate in degrees per second, controller frame.
 * @param accel_g Acceleration in g (1.0 at rest), controller frame. Reads +Z when lying flat.
 * @param dt_s Seconds since the previous sample. Gaps over 50 ms are clamped.
 */
void nwii_aim_update(nwii_aim_s *aim, const float gyro_dps[3], const float accel_g[3], float dt_s);

/**
 * @brief Move the cursor directly, for a stick or touch surface alongside the gyro.
 *
 * @param aim State.
 * @param dx Change in x (screen half-widths). Positive moves right.
 * @param dy Change in y (screen half-heights). Positive moves up.
 */
void nwii_aim_nudge(nwii_aim_s *aim, float dx, float dy);

/**
 * @brief Put the cursor in the centre of the screen, keeping the learned gyro offset.
 */
void nwii_aim_recenter(nwii_aim_s *aim);

/**
 * @brief Write the IR points for the current aim (position and roll).
 *
 * @param aim State.
 * @param out NWII_IR_POINT_COUNT points, e.g. nwii_input_s.ir.
 * @param with_roll false keeps the sensor bar level (e.g. a remote held sideways).
 */
void nwii_aim_to_ir(const nwii_aim_s *aim, nwii_ir_point_s out[NWII_IR_POINT_COUNT], bool with_roll);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_AIM_H */
