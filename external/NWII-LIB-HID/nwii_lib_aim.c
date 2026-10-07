/**
 * @file nwii_lib_aim.c
 * @brief Gyro + accelerometer fusion for pointer aiming.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#include "nwii_lib_aim.h"
#include "nwii_lib_ir.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define NWII_AIM_DEG_TO_RAD   0.017453293f
#define NWII_AIM_MAX_DT_S     0.05f

// The accelerometer only reads gravity while the controller is not being swung; outside this
// band of magnitudes (in g) the gravity estimate runs on the gyro alone
#define NWII_AIM_ACCEL_MIN_G  0.8f
#define NWII_AIM_ACCEL_MAX_G  1.2f

// Held still also requires a steady 1 g, so a slow, smooth pan is not mistaken for gyro offset
#define NWII_AIM_STILL_ACCEL_G 0.05f

static inline float _nwii_aim_absf(float v)
{
    return (v < 0.0f) ? -v : v;
}

static inline float _nwii_aim_clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static inline float _nwii_aim_dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline void _nwii_aim_cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static bool _nwii_aim_normalize(float v[3])
{
    const float len = sqrtf(_nwii_aim_dot(v, v));
    if (len < 1e-6f) return false;
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
    return true;
}

static inline float _nwii_aim_deadband(float v, float band)
{
    if (_nwii_aim_absf(v) <= band) return 0.0f;
    return (v > 0.0f) ? (v - band) : (v + band);
}

// Remove the part of v along the unit vector up, leaving its horizontal component
static inline void _nwii_aim_horizontal(const float v[3], const float up[3], float out[3])
{
    const float d = _nwii_aim_dot(v, up);
    out[0] = v[0] - d * up[0];
    out[1] = v[1] - d * up[1];
    out[2] = v[2] - d * up[2];
}

void nwii_aim_default_config(nwii_aim_config_s *cfg)
{
    if (cfg == NULL) return;

    cfg->yaw_range_deg   = 12.5f;
    cfg->pitch_range_deg = 11.5f;
    cfg->deadband_dps    = 0.75f;
    cfg->still_dps       = 2.0f;
    cfg->still_s         = 0.5f;
    cfg->bias_tau_s      = 1.0f;
    cfg->gravity_tau_s   = 0.5f;
}

void nwii_aim_init(nwii_aim_s *aim, const nwii_aim_config_s *cfg)
{
    if (aim == NULL) return;

    memset(aim, 0, sizeof(*aim));
    if (cfg != NULL)
        aim->cfg = *cfg;
    else
        nwii_aim_default_config(&aim->cfg);

    aim->up[2] = 1.0f;
}

// Learn the gyro's resting offset while the controller is held still, so the cursor does not
// creep. Deliberate slow aiming never stays under the threshold long enough to be absorbed.
static void _nwii_aim_track_bias(nwii_aim_s *aim, const float gyro_dps[3], float accel_mag_g, float dt)
{
    bool still = _nwii_aim_absf(accel_mag_g - 1.0f) < NWII_AIM_STILL_ACCEL_G;
    for (int i = 0; i < 3 && still; i++)
    {
        still = _nwii_aim_absf(gyro_dps[i] - aim->bias_dps[i]) < aim->cfg.still_dps;
    }

    if (!still)
    {
        aim->still_s = 0.0f;
        return;
    }

    if (aim->still_s < aim->cfg.still_s)
    {
        aim->still_s += dt;
        return;
    }

    const float k = _nwii_aim_clampf(dt / aim->cfg.bias_tau_s, 0.0f, 1.0f);
    for (int i = 0; i < 3; i++)
    {
        aim->bias_dps[i] += (gyro_dps[i] - aim->bias_dps[i]) * k;
    }
}

// Carry the up vector along with the gyro, then pull it gently toward the accelerometer
static void _nwii_aim_track_gravity(nwii_aim_s *aim, const float w_rad[3], const float accel_g[3],
                                    float accel_mag_g, float dt)
{
    float accel_up[3] = {accel_g[0], accel_g[1], accel_g[2]};
    const bool accel_ok = (accel_mag_g > NWII_AIM_ACCEL_MIN_G) && (accel_mag_g < NWII_AIM_ACCEL_MAX_G) &&
                          _nwii_aim_normalize(accel_up);

    if (!aim->up_valid)
    {
        if (!accel_ok) return;
        memcpy(aim->up, accel_up, sizeof(aim->up));
        aim->up_valid = true;
        return;
    }

    // A direction fixed in the world turns the opposite way in the controller frame: du = -w x u
    float turn[3];
    _nwii_aim_cross(w_rad, aim->up, turn);
    for (int i = 0; i < 3; i++)
    {
        aim->up[i] -= turn[i] * dt;
    }

    if (accel_ok)
    {
        const float k = _nwii_aim_clampf(dt / aim->cfg.gravity_tau_s, 0.0f, 1.0f);
        for (int i = 0; i < 3; i++)
        {
            aim->up[i] += (accel_up[i] - aim->up[i]) * k;
        }
    }

    if (!_nwii_aim_normalize(aim->up))
    {
        aim->up[0] = 0.0f;
        aim->up[1] = 0.0f;
        aim->up[2] = 1.0f;
    }
}

void nwii_aim_update(nwii_aim_s *aim, const float gyro_dps[3], const float accel_g[3], float dt_s)
{
    if (aim == NULL || gyro_dps == NULL || accel_g == NULL) return;

    const float dt = _nwii_aim_clampf(dt_s, 0.0f, NWII_AIM_MAX_DT_S);
    const float accel_mag_g = sqrtf(_nwii_aim_dot(accel_g, accel_g));

    _nwii_aim_track_bias(aim, gyro_dps, accel_mag_g, dt);

    float w_dps[3];
    float w_rad[3];
    for (int i = 0; i < 3; i++)
    {
        w_dps[i] = gyro_dps[i] - aim->bias_dps[i];
        w_rad[i] = w_dps[i] * NWII_AIM_DEG_TO_RAD;
    }

    _nwii_aim_track_gravity(aim, w_rad, accel_g, accel_mag_g, dt);
    if (!aim->up_valid) return;

    // Pointing direction: the controller's front edge (-Y) when flat, its face (+Z) when stood up
    // facing the screen. The horizontal parts of both blend smoothly between those grips.
    static const float front_edge[3] = {0.0f, -1.0f, 0.0f};
    static const float face[3] = {0.0f, 0.0f, 1.0f};
    float fwd_a[3];
    float fwd_b[3];
    _nwii_aim_horizontal(front_edge, aim->up, fwd_a);
    _nwii_aim_horizontal(face, aim->up, fwd_b);
    float fwd[3] = {fwd_a[0] + fwd_b[0], fwd_a[1] + fwd_b[1], fwd_a[2] + fwd_b[2]};

    // Aim right is clockwise seen from above (negative about up). Aim up turns the pointing
    // direction toward up, which is rotation about fwd x up.
    const float yaw_dps = -_nwii_aim_dot(w_dps, aim->up);
    float pitch_dps = 0.0f;

    float pitch_axis[3];
    if (_nwii_aim_normalize(fwd))
    {
        _nwii_aim_cross(fwd, aim->up, pitch_axis);
        pitch_dps = _nwii_aim_dot(w_dps, pitch_axis);
    }

    aim->x += _nwii_aim_deadband(yaw_dps, aim->cfg.deadband_dps) * dt / aim->cfg.yaw_range_deg;
    aim->y += _nwii_aim_deadband(pitch_dps, aim->cfg.deadband_dps) * dt / aim->cfg.pitch_range_deg;

    // The edges absorb further rotation, so turning back from an edge recentres naturally
    aim->x = _nwii_aim_clampf(aim->x, -1.0f, 1.0f);
    aim->y = _nwii_aim_clampf(aim->y, -1.0f, 1.0f);

    // Roll about the pointing direction: how far the left side (+X) has risen. Measured against
    // the controller's "top" for either grip (+Z flat, -Y stood up), so it stays within +/-90 deg.
    aim->roll_rad = atan2f(aim->up[0], sqrtf(aim->up[1] * aim->up[1] + aim->up[2] * aim->up[2]));
}

void nwii_aim_nudge(nwii_aim_s *aim, float dx, float dy)
{
    if (aim == NULL) return;

    aim->x = _nwii_aim_clampf(aim->x + dx, -1.0f, 1.0f);
    aim->y = _nwii_aim_clampf(aim->y + dy, -1.0f, 1.0f);
}

void nwii_aim_recenter(nwii_aim_s *aim)
{
    if (aim == NULL) return;

    aim->x = 0.0f;
    aim->y = 0.0f;
}

void nwii_aim_to_ir(const nwii_aim_s *aim, nwii_ir_point_s out[NWII_IR_POINT_COUNT], bool with_roll)
{
    if (aim == NULL) return;

    nwii_ir_set_pointer_rotated(out, aim->x, aim->y, with_roll ? aim->roll_rad : 0.0f);
}
