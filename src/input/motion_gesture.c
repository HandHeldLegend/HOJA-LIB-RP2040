#include "input/motion_gesture.h"

#include "motion_gesture_flicks.h"

#include <math.h>

// Flicks replay real flicks recorded on a GCU-2 (scripts/imu_record.py, turned into tables by
// scripts/imu_flick_templates.py): the push, the harder stop and rebound, the pull toward the
// wrist from swinging around it, and the wrist turning out and back to where it started. One
// flick per press, however long it is held; games that only check for a shake take any
// direction.
typedef struct
{
    const motion_flick_sample_s *samples;
    uint16_t                     count;
} motion_flick_table_s;

#define MOTION_FLICK_TABLE(t) {.samples = (t), .count = (uint16_t)(sizeof(t) / sizeof((t)[0]))}

static const motion_flick_table_s _flick_tables[MOTION_GESTURE_MAX] = {
    [MOTION_GESTURE_FLICK_UP]    = MOTION_FLICK_TABLE(_flick_up),
    [MOTION_GESTURE_FLICK_DOWN]  = MOTION_FLICK_TABLE(_flick_down),
    [MOTION_GESTURE_FLICK_LEFT]  = MOTION_FLICK_TABLE(_flick_left),
    [MOTION_GESTURE_FLICK_RIGHT] = MOTION_FLICK_TABLE(_flick_right),
};

static inline uint64_t _motion_flick_duration_us(motion_gesture_t gesture)
{
    return (uint64_t)(_flick_tables[gesture].count - 1u) * MOTION_FLICK_STEP_US;
}

static const float _flick_ref_up[3] = MOTION_FLICK_REF_UP;

static void _motion_rot_identity(float r[9])
{
    for (int i = 0; i < 9; i++)
        r[i] = (i % 4 == 0) ? 1.0f : 0.0f;
}

// Smallest rotation taking the recorded up direction to the current one. Which way the controller
// faces around gravity cannot be told from gravity alone, so the turn about it is left out: up
// and down follow the ground, left and right stay level toward the controller's sides.
static void _motion_rot_from_up(const float up[3], float r[9])
{
    _motion_rot_identity(r);
    if (!up)
        return;

    const float len = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (len < 500.0f) // Under half a g: falling or no reading, keep the controller's axes
        return;

    const float b[3] = {up[0] / len, up[1] / len, up[2] / len};
    const float *a = _flick_ref_up;

    const float c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    const float v[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};

    if (c < -0.999f)
    {
        // Upside down: half a turn about the controller's Y axis
        r[0] = -1.0f;
        r[8] = -1.0f;
        return;
    }

    // Rodrigues: R = I + [v]x + [v]x^2 / (1 + c)
    const float k = 1.0f / (1.0f + c);
    r[0] = 1.0f - k * (v[1] * v[1] + v[2] * v[2]);
    r[1] = -v[2] + k * v[0] * v[1];
    r[2] =  v[1] + k * v[0] * v[2];
    r[3] =  v[2] + k * v[0] * v[1];
    r[4] = 1.0f - k * (v[0] * v[0] + v[2] * v[2]);
    r[5] = -v[0] + k * v[1] * v[2];
    r[6] = -v[1] + k * v[0] * v[2];
    r[7] =  v[0] + k * v[1] * v[2];
    r[8] = 1.0f - k * (v[0] * v[0] + v[1] * v[1]);
}

// Interpolate between the two samples around elapsed and turn the result into the flick's pose
static void _motion_flick(motion_gesture_t gesture, const float rot[9], uint64_t elapsed, motion_gesture_out_s *out)
{
    const motion_flick_table_s *table = &_flick_tables[gesture];

    const uint32_t i = (uint32_t)(elapsed / MOTION_FLICK_STEP_US);
    if (i + 1u >= table->count)
        return;

    const float f = (float)(elapsed % MOTION_FLICK_STEP_US) / (float)MOTION_FLICK_STEP_US;
    const motion_flick_sample_s *a = &table->samples[i];
    const motion_flick_sample_s *b = &table->samples[i + 1u];

    float accel[3], gyro[3];
    for (int k = 0; k < 3; k++)
    {
        accel[k] = (float)a->accel_mg[k] + f * (float)(b->accel_mg[k] - a->accel_mg[k]);
        gyro[k]  = (float)a->gyro_dps[k] + f * (float)(b->gyro_dps[k] - a->gyro_dps[k]);
    }

    for (int r = 0; r < 3; r++)
    {
        out->accel_mg[r] += rot[r * 3] * accel[0] + rot[r * 3 + 1] * accel[1] + rot[r * 3 + 2] * accel[2];
        out->gyro_dps[r] += rot[r * 3] * gyro[0] + rot[r * 3 + 1] * gyro[1] + rot[r * 3 + 2] * gyro[2];
    }
}

static void _motion_gesture_update_one(motion_gesture_state_s *state, motion_gesture_t gesture, bool pressed,
                                       const float up[3], uint64_t now_us, motion_gesture_out_s *out)
{
    if (!state || !out || gesture >= MOTION_GESTURE_MAX)
        return;

    if (pressed && !state->was_pressed && !state->active)
    {
        state->active   = true;
        state->start_us = now_us;
        state->end_us   = now_us + _motion_flick_duration_us(gesture);
        // Fixed for the whole flick so it cannot wobble partway through
        _motion_rot_from_up(up, state->rot);
    }
    state->was_pressed = pressed;

    if (!state->active)
        return;

    if (now_us >= state->end_us)
    {
        state->active = false;
        return;
    }

    _motion_flick(gesture, state->rot, now_us - state->start_us, out);
}

motion_gesture_out_s motion_gesture_update(motion_gesture_set_s *set, const bool pressed[MOTION_GESTURE_MAX],
                                           const float up[3], uint64_t now_us)
{
    motion_gesture_out_s out = {0};
    if (!set || !pressed)
        return out;

    for (int g = 0; g < MOTION_GESTURE_MAX; g++)
        _motion_gesture_update_one(&set->gesture[g], (motion_gesture_t)g, pressed[g], up, now_us, &out);

    return out;
}
