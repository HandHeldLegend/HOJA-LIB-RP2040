#ifndef INPUT_MOTION_GESTURE_H
#define INPUT_MOTION_GESTURE_H

#include <stdint.h>
#include <stdbool.h>

// Motion gestures bound to buttons: each one plays a short accelerometer and gyro pattern that a
// core adds on top of the IMU data it reports. Cores add it at the very end, after
// any fusion, so the pattern never disturbs pointer aim or quaternion integration, and it still
// plays when motion is turned off or the board has no IMU.
//
// Offsets are in HOJA's standardized IMU frame (the same on every board): accel +X toward the
// gamepad's left, +Y toward the player, +Z up out of its face; nose up reads negative gyro X and
// a right turn negative gyro Z. Each core maps that frame to its report the way it maps the IMU.

typedef enum
{
    MOTION_GESTURE_FLICK_UP,    // Wrist flicks up
    MOTION_GESTURE_FLICK_DOWN,  // Wrist flicks down
    MOTION_GESTURE_FLICK_LEFT,  // Wrist turns left
    MOTION_GESTURE_FLICK_RIGHT, // Wrist turns right
    MOTION_GESTURE_MAX,
} motion_gesture_t;

typedef struct
{
    float accel_mg[3];
    float gyro_dps[3];
} motion_gesture_out_s;

typedef struct
{
    bool     was_pressed;
    bool     active;
    uint64_t start_us;
    uint64_t end_us;  // End of the pattern in progress
    float    rot[9];  // Recorded pose -> pose at the start of this flick (row-major)
} motion_gesture_state_s;

typedef struct
{
    motion_gesture_state_s gesture[MOTION_GESTURE_MAX];
} motion_gesture_set_s;

// Advance every gesture in the set and return their combined offset (zero when all are idle).
// up is which way is up right now (the accelerometer's gravity reading in mg, HOJA frame): each
// flick is turned from the pose it was recorded in to this one when it starts, so flick up always
// moves away from the ground however the controller is held. Pass NULL to play flicks in the
// controller's own axes.
motion_gesture_out_s motion_gesture_update(motion_gesture_set_s *set, const bool pressed[MOTION_GESTURE_MAX],
                                           const float up[3], uint64_t now_us);

// True while any gesture in the set is playing or its button is still held
bool motion_gesture_busy(const motion_gesture_set_s *set);

#endif
