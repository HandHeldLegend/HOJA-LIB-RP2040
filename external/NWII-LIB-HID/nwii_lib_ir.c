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
    if (out == NULL)
    {
        return;
    }

    nwii_ir_clear(out);

    x = _nwii_ir_clampf(x, -1.0f, 1.0f);
    y = _nwii_ir_clampf(y, -1.0f, 1.0f);

    // The camera image is mirrored relative to the pointing direction: aiming right moves the
    // sensor bar toward the left of the scene, which the remote reports as larger x. Aiming up
    // likewise reports larger y.
    const int32_t cx = (int32_t)(NWII_IR_RES_X / 2u) + (int32_t)(x * (float)NWII_IR_POINTER_RANGE_X);
    const int32_t cy = (int32_t)(NWII_IR_RES_Y / 2u) + (int32_t)(y * (float)NWII_IR_POINTER_RANGE_Y);
    const int32_t half = NWII_IR_BAR_SEPARATION / 2;

    // Point order follows the camera's left-to-right scan
    out[0].x = (uint16_t)(cx - half);
    out[0].y = (uint16_t)cy;
    out[0].size = NWII_IR_POINT_SIZE;
    out[0].visible = true;

    out[1].x = (uint16_t)(cx + half);
    out[1].y = (uint16_t)cy;
    out[1].size = NWII_IR_POINT_SIZE;
    out[1].visible = true;
}
