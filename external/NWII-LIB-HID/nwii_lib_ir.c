/**
 * @file nwii_lib_ir.c
 * @brief Virtual sensor bar for pointer emulation.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#include "nwii_lib_ir.h"

#include <stddef.h>
#include <math.h>

static inline float _nwii_ir_clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void nwii_ir_clear(nwii_ir_point_s out[NWII_IR_POINT_COUNT])
{
    if (out == NULL)
    {
        return;
    }

    for (uint8_t i = 0; i < NWII_IR_POINT_COUNT; i++)
    {
        out[i].x = 0x3FF;
        out[i].y = 0x3FF;
        out[i].size = 0x0F;
        out[i].visible = false;
    }
}

void nwii_ir_set_pointer(nwii_ir_point_s out[NWII_IR_POINT_COUNT], float x, float y)
{
    nwii_ir_set_pointer_rotated(out, x, y, 0.0f);
}

static void _nwii_ir_set_point(nwii_ir_point_s *point, float x, float y)
{
    // A point pushed past the camera's edge drops out, as it would on a real remote
    if ((x < 0.0f) || (x > (float)(NWII_IR_RES_X - 1u)) || (y < 0.0f) || (y > (float)(NWII_IR_RES_Y - 1u)))
    {
        return;
    }

    point->x = (uint16_t)(x + 0.5f);
    point->y = (uint16_t)(y + 0.5f);
    point->size = NWII_IR_POINT_SIZE;
    point->visible = true;
}

void nwii_ir_set_pointer_rotated(nwii_ir_point_s out[NWII_IR_POINT_COUNT], float x, float y, float roll_rad)
{
    if (out == NULL)
    {
        return;
    }

    nwii_ir_clear(out);

    x = _nwii_ir_clampf(x, -1.0f, 1.0f);
    y = _nwii_ir_clampf(y, -1.0f, 1.0f);

    // The camera sees the sensor bar move opposite to the aim: aiming right reports lower x and
    // aiming up reports lower y, as on a real remote (verified on a Wii). Seen from behind the
    // remote that makes camera space x-right, y-up.
    const float dx = -x * (float)NWII_IR_POINTER_RANGE_X;
    const float dy = -y * (float)NWII_IR_POINTER_RANGE_Y;
    const float half = (float)NWII_IR_BAR_SEPARATION / 2.0f;

    // Rolling the remote clockwise turns the whole camera image counter-clockwise about its
    // centre. The Wii undoes that rotation using the angle between the dots, so the cursor stays
    // put and only its tilt changes.
    const float c = cosf(roll_rad);
    const float s = sinf(roll_rad);

    const float mx = (float)(NWII_IR_RES_X / 2u) + (dx * c - dy * s);
    const float my = (float)(NWII_IR_RES_Y / 2u) + (dx * s + dy * c);

    // Point order follows the camera's left-to-right scan when level
    _nwii_ir_set_point(&out[0], mx - half * c, my - half * s);
    _nwii_ir_set_point(&out[1], mx + half * c, my + half * s);
}
