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
 * The camera sees roughly 42 x 31 degrees (about 24 px per degree). These ranges correspond to
 * about +/-12.5 degrees of yaw and +/-10 degrees of pitch, which the Wii maps to the edges of
 * the screen.
 */
#define NWII_IR_POINTER_RANGE_X     305
#define NWII_IR_POINTER_RANGE_Y     244

/** @brief Gap between the two sensor bar LEDs as seen from ~2.5 m (20 cm bar). */
#define NWII_IR_BAR_SEPARATION      112

/** @brief Point size reported for each LED. */
#define NWII_IR_POINT_SIZE          2

/**
 * @brief Fill IR points for a cursor position.
 *
 * Writes two visible points (the sensor bar LEDs) and marks the other two invisible. Pointing
 * right moves the camera points toward higher x and pointing up moves them toward higher y,
 * matching what a real remote reports.
 *
 * @param out NWII_IR_POINT_COUNT points to fill.
 * @param x Cursor x, -1.0 (left screen edge) .. +1.0 (right edge). Values are clamped.
 * @param y Cursor y, -1.0 (bottom edge) .. +1.0 (top edge). Values are clamped.
 */
void nwii_ir_set_pointer(nwii_ir_point_s out[NWII_IR_POINT_COUNT], float x, float y);

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
