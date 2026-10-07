/**
 * @file nwii_lib_ir.h
 * @brief Virtual sensor bar: converts a cursor position into the IR points a Wii Remote camera
 *        would report, so pointer input can come from a gyro, stick or touch surface.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef NWII_LIB_IR_H
#define NWII_LIB_IR_H

#include <stdint.h>
#include <stdbool.h>

#include "nwii_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Camera-space travel of the sensor bar centre for a cursor at the screen edge.
 *
 * The camera sees roughly 42 x 31 degrees (about 24 px per degree), so these correspond to about
 * +/-12.5 degrees of yaw and +/-11.5 degrees of pitch.
 */
#define NWII_IR_POINTER_RANGE_X     305
#define NWII_IR_POINTER_RANGE_Y     282

/**
 * @brief Vertical camera offset of the sensor bar for a centred cursor.
 *
 * The Wii does not centre the cursor on the bar itself: a remote aimed at the middle of the screen
 * sees the bar about 10 cm off its aim (about 56 px at 2.5 m). Without this, a centred cursor
 * lands below the middle of the screen.
 */
#define NWII_IR_POINTER_OFFSET_Y    56

/** @brief Gap between the two sensor bar LEDs as seen from ~2.5 m (20 cm bar). */
#define NWII_IR_BAR_SEPARATION      112

/** @brief Point size reported for each LED. */
#define NWII_IR_POINT_SIZE          2

/**
 * @brief Fill IR points for a cursor position.
 *
 * Writes two visible points (the sensor bar LEDs) and marks the other two invisible. Pointing
 * right moves the camera points toward lower x and pointing up moves them toward lower y,
 * matching what a real remote reports.
 *
 * @param out NWII_IR_POINT_COUNT points to fill.
 * @param x Cursor x, -1.0 (left screen edge) .. +1.0 (right edge). Values are clamped.
 * @param y Cursor y, -1.0 (bottom edge) .. +1.0 (top edge). Values are clamped.
 */
void nwii_ir_set_pointer(nwii_ir_point_s out[NWII_IR_POINT_COUNT], float x, float y);

/**
 * @brief Fill IR points for a cursor position with the remote rolled about its pointing axis.
 *
 * The Wii reads cursor tilt from the angle between the two sensor bar dots, so this rotates the
 * whole camera image (bar position and bar angle) the way a real remote's camera sees it. Take
 * roll from the same accelerometer data sent in the input report: the Wii also uses the
 * accelerometer to tell the two dots apart. A dot rotated past the camera's edge is left
 * invisible.
 *
 * @param out NWII_IR_POINT_COUNT points to fill.
 * @param x Cursor x, -1.0 (left screen edge) .. +1.0 (right edge). Values are clamped.
 * @param y Cursor y, -1.0 (bottom edge) .. +1.0 (top edge). Values are clamped.
 * @param roll_rad Roll in radians, as atan2f(accel_x, accel_z) of the remote accelerometer values
 *                 you report (checked on a Wii; nwii_aim_s.roll_rad uses the same sign).
 */
void nwii_ir_set_pointer_rotated(nwii_ir_point_s out[NWII_IR_POINT_COUNT], float x, float y, float roll_rad);

/**
 * @brief Mark every IR point invisible (remote pointed away from the screen).
 *
 * @param out NWII_IR_POINT_COUNT points to clear.
 */
void nwii_ir_clear(nwii_ir_point_s out[NWII_IR_POINT_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_IR_H */
