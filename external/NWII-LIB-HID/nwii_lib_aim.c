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

// Player-space yaw: boost for a tilted grip (GyroWiki's recommended 1.41)
#define NWII_AIM_YAW_RELAX    1.41f

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

    // Aim like a real remote: the cursor follows where the front edge (-Y) points, so up/down and
    // left/right stay true to the room however the controller is rolled in the hand.
    //   left/right: rotation about the real vertical (aim right is clockwise seen from above,
    //               negative about up)
    //   up/down:    the front edge turning toward up, i.e. rotation about front x up, which is
    //               (-up.z, 0, up.x) normalized by the lean below
    // "Lean" is how far up is from the front edge; pointed straight up or down it vanishes and
    // world space has no answer (turning becomes a twist about the pointing direction).
    const float lean = sqrtf(aim->up[0] * aim->up[0] + aim->up[2] * aim->up[2]);
    const float world_weight = _nwii_aim_clampf((lean - 0.2f) / 0.3f, 0.0f, 1.0f);

    float world_yaw_dps = 0.0f;
    float world_pitch_dps = 0.0f;
    if (lean > 1e-3f)
    {
        world_yaw_dps = -_nwii_aim_dot(w_dps, aim->up);
        world_pitch_dps = (aim->up[0] * w_dps[2] - aim->up[2] * w_dps[0]) / lean;
    }

    // Near vertical, blend into player space (after the GyroWiki): up/down is the controller's own
    // pitch, left/right is rotation about the vertical from its yaw and roll axes only, with the
    // relax factor letting a tilted grip reach full speed, capped at the actual rotation rate
    const float player_world_yaw = w_dps[1] * aim->up[1] + w_dps[2] * aim->up[2];
    const float yaw_cap = sqrtf(w_dps[1] * w_dps[1] + w_dps[2] * w_dps[2]);
    float yaw_mag = _nwii_aim_absf(player_world_yaw) * NWII_AIM_YAW_RELAX;
    if (yaw_mag > yaw_cap) yaw_mag = yaw_cap;
    const float player_yaw_dps = (player_world_yaw > 0.0f) ? -yaw_mag : yaw_mag;
    const float player_pitch_dps = -w_dps[0];

    const float yaw_dps = world_weight * world_yaw_dps + (1.0f - world_weight) * player_yaw_dps;
    const float pitch_dps = world_weight * world_pitch_dps + (1.0f - world_weight) * player_pitch_dps;

    aim->x += _nwii_aim_deadband(yaw_dps, aim->cfg.deadband_dps) * dt / aim->cfg.yaw_range_deg;
    aim->y += _nwii_aim_deadband(pitch_dps, aim->cfg.deadband_dps) * dt / aim->cfg.pitch_range_deg;

    // The edges absorb further rotation, so turning back from an edge recentres naturally
    aim->x = _nwii_aim_clampf(aim->x, -1.0f, 1.0f);
    aim->y = _nwii_aim_clampf(aim->y, -1.0f, 1.0f);

    // Roll about the front edge: how far the right side (-X) has risen against the face (+Z), the
    // sign nwii_ir_set_pointer_rotated() expects (checked on a Wii). Pointed straight up or down
    // there is no roll to read, so it fades to level there.
    aim->roll_rad = atan2f(-aim->up[0], _nwii_aim_absf(aim->up[2])) * world_weight;
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

    // The pose at recentre becomes "level, aimed at the screen" (see nwii_aim_level_accel)
    if (aim->up_valid)
        aim->level_pitch_rad = asinf(_nwii_aim_clampf(-aim->up[1], -1.0f, 1.0f));
}

void nwii_aim_level_accel(const nwii_aim_s *aim, const float accel[3], float out[3])
{
    if (aim == NULL || accel == NULL || out == NULL) return;

    // Undo the front-edge pitch captured at recentre: rotate about +X so that pose reads flat
    const float c = cosf(aim->level_pitch_rad);
    const float s = sinf(aim->level_pitch_rad);
    const float y = accel[1];
    const float z = accel[2];

    out[0] = accel[0];
    out[1] = y * c + z * s;
    out[2] = z * c - y * s;
}

void nwii_aim_to_ir(const nwii_aim_s *aim, nwii_ir_point_s out[NWII_IR_POINT_COUNT], bool with_roll)
{
    if (aim == NULL) return;

    nwii_ir_set_pointer_rotated(out, aim->x, aim->y, with_roll ? aim->roll_rad : 0.0f);
}
