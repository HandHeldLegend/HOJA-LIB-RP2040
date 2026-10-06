/**
 * @file nwii_lib_extension.c
 * @brief Nunchuk and Classic Controller register images and data encoding.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#include "nwii_lib_extension.h"

#include <stddef.h>
#include <string.h>

/* Nunchuk accelerometer calibration (10-bit) and stick range (8-bit). */
#define NWII_NUNCHUK_ACCEL_ZERO     0x200u
#define NWII_NUNCHUK_ACCEL_ONE_G    0x2CCu
#define NWII_NUNCHUK_STICK_CENTER   0x80
#define NWII_NUNCHUK_STICK_RANGE    0x60

/* Classic sticks are 6-bit (left) and 5-bit (right); triggers are 5-bit. */
#define NWII_CLASSIC_LS_CENTER      32
#define NWII_CLASSIC_LS_RANGE       24
#define NWII_CLASSIC_RS_CENTER      16
#define NWII_CLASSIC_RS_RANGE       12
#define NWII_CLASSIC_TRIGGER_MAX    31u

/* Identification bytes read from 0xFA..0xFF. */
static const uint8_t _nwii_id_nunchuk[6]     = {0x00, 0x00, 0xA4, 0x20, 0x00, 0x00};
static const uint8_t _nwii_id_classic[6]     = {0x00, 0x00, 0xA4, 0x20, 0x01, 0x01};
static const uint8_t _nwii_id_classic_pro[6] = {0x01, 0x00, 0xA4, 0x20, 0x01, 0x01};

static inline int32_t _nwii_clamp(int32_t v, int32_t lo, int32_t hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

/* Scale a 0..4095 (2048 centre) axis to centre +/- range. */
static inline uint8_t _nwii_scale_axis(uint16_t axis, int32_t center, int32_t range, int32_t max)
{
    const int32_t delta = (int32_t)axis - NWII_STICK_CENTER;
    return (uint8_t)_nwii_clamp(center + (delta * range) / NWII_STICK_CENTER, 0, max);
}

/* Calibration blocks end in two checksum bytes: sum(bytes 0..13) + 0x55, then that + 0x55. */
static void _nwii_calibration_checksum(uint8_t cal[16])
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < 14u; i++)
    {
        sum = (uint8_t)(sum + cal[i]);
    }
    cal[14] = (uint8_t)(sum + 0x55u);
    cal[15] = (uint8_t)(cal[14] + 0x55u);
}

static void _nwii_nunchuk_calibration(uint8_t cal[16])
{
    const uint8_t zero = (uint8_t)(NWII_NUNCHUK_ACCEL_ZERO >> 2);
    const uint8_t one_g = (uint8_t)(NWII_NUNCHUK_ACCEL_ONE_G >> 2);

    cal[0] = zero;  cal[1] = zero;  cal[2] = zero;  cal[3] = 0x00;
    cal[4] = one_g; cal[5] = one_g; cal[6] = one_g; cal[7] = 0x00;

    // Stick X then Y: max, min, centre
    cal[8]  = NWII_NUNCHUK_STICK_CENTER + NWII_NUNCHUK_STICK_RANGE;
    cal[9]  = NWII_NUNCHUK_STICK_CENTER - NWII_NUNCHUK_STICK_RANGE;
    cal[10] = NWII_NUNCHUK_STICK_CENTER;
    cal[11] = NWII_NUNCHUK_STICK_CENTER + NWII_NUNCHUK_STICK_RANGE;
    cal[12] = NWII_NUNCHUK_STICK_CENTER - NWII_NUNCHUK_STICK_RANGE;
    cal[13] = NWII_NUNCHUK_STICK_CENTER;

    _nwii_calibration_checksum(cal);
}

static void _nwii_classic_calibration(uint8_t cal[16])
{
    // Values are stored left-aligned to 8 bits: left stick << 2, right stick << 3.
    // Left X, left Y, right X, right Y: max, min, centre
    for (uint8_t i = 0; i < 2u; i++)
    {
        cal[i * 3u + 0u] = (uint8_t)((NWII_CLASSIC_LS_CENTER + NWII_CLASSIC_LS_RANGE) << 2);
        cal[i * 3u + 1u] = (uint8_t)((NWII_CLASSIC_LS_CENTER - NWII_CLASSIC_LS_RANGE) << 2);
        cal[i * 3u + 2u] = (uint8_t)(NWII_CLASSIC_LS_CENTER << 2);
    }
    for (uint8_t i = 2; i < 4u; i++)
    {
        cal[i * 3u + 0u] = (uint8_t)((NWII_CLASSIC_RS_CENTER + NWII_CLASSIC_RS_RANGE) << 3);
        cal[i * 3u + 1u] = (uint8_t)((NWII_CLASSIC_RS_CENTER - NWII_CLASSIC_RS_RANGE) << 3);
        cal[i * 3u + 2u] = (uint8_t)(NWII_CLASSIC_RS_CENTER << 3);
    }

    // Trigger rest values (L, R)
    cal[12] = 0x00;
    cal[13] = 0x00;

    _nwii_calibration_checksum(cal);
}

void nwii_extension_reset_registers(uint8_t reg[NWII_EXTENSION_REG_SIZE], nwii_extension_t extension)
{
    if (reg == NULL)
    {
        return;
    }

    if (extension == NWII_EXTENSION_NONE || extension >= NWII_EXTENSION_MAX)
    {
        memset(reg, 0xFF, NWII_EXTENSION_REG_SIZE);
        return;
    }

    memset(reg, 0x00, NWII_EXTENSION_REG_SIZE);

    const uint8_t *id = _nwii_id_nunchuk;
    uint8_t cal[16] = {0};

    switch (extension)
    {
    default:
    case NWII_EXTENSION_NUNCHUK:
        id = _nwii_id_nunchuk;
        _nwii_nunchuk_calibration(cal);
        break;

    case NWII_EXTENSION_CLASSIC:
        id = _nwii_id_classic;
        _nwii_classic_calibration(cal);
        break;

    case NWII_EXTENSION_CLASSIC_PRO:
        id = _nwii_id_classic_pro;
        _nwii_classic_calibration(cal);
        break;
    }

    // Calibration is stored twice (primary + backup copy)
    memcpy(&reg[NWII_EXTENSION_REG_CAL], cal, sizeof(cal));
    memcpy(&reg[NWII_EXTENSION_REG_CAL + 0x10u], cal, sizeof(cal));

    reg[NWII_EXTENSION_REG_CRYPT] = NWII_EXTENSION_CRYPT_OFF;
    memcpy(&reg[NWII_EXTENSION_REG_ID], id, 6);
}

uint16_t nwii_extension_accel_to_10bit(int16_t mg, uint16_t zero, uint16_t one_g)
{
    const int32_t span = (int32_t)one_g - (int32_t)zero;
    return (uint16_t)_nwii_clamp((int32_t)zero + ((int32_t)mg * span) / NWII_ACCEL_1G_MG, 0, 1023);
}

static void _nwii_encode_nunchuk(const nwii_nunchuk_s *n, uint8_t out[NWII_EXTENSION_DATA_LEN])
{
    const uint16_t ax = nwii_extension_accel_to_10bit(n->accel_x, NWII_NUNCHUK_ACCEL_ZERO, NWII_NUNCHUK_ACCEL_ONE_G);
    const uint16_t ay = nwii_extension_accel_to_10bit(n->accel_y, NWII_NUNCHUK_ACCEL_ZERO, NWII_NUNCHUK_ACCEL_ONE_G);
    const uint16_t az = nwii_extension_accel_to_10bit(n->accel_z, NWII_NUNCHUK_ACCEL_ZERO, NWII_NUNCHUK_ACCEL_ONE_G);

    out[0] = _nwii_scale_axis(n->stick_x, NWII_NUNCHUK_STICK_CENTER, NWII_NUNCHUK_STICK_RANGE, 0xFF);
    out[1] = _nwii_scale_axis(n->stick_y, NWII_NUNCHUK_STICK_CENTER, NWII_NUNCHUK_STICK_RANGE, 0xFF);
    out[2] = (uint8_t)(ax >> 2);
    out[3] = (uint8_t)(ay >> 2);
    out[4] = (uint8_t)(az >> 2);

    // Buttons are active low
    out[5] = (uint8_t)((n->z ? 0u : 0x01u) |
                       (n->c ? 0u : 0x02u) |
                       ((ax & 0x03u) << 2) |
                       ((ay & 0x03u) << 4) |
                       ((az & 0x03u) << 6));
}

static void _nwii_encode_classic(const nwii_classic_s *c, uint8_t out[NWII_EXTENSION_DATA_LEN])
{
    const uint8_t lx = _nwii_scale_axis(c->ls_x, NWII_CLASSIC_LS_CENTER, NWII_CLASSIC_LS_RANGE, 63);
    const uint8_t ly = _nwii_scale_axis(c->ls_y, NWII_CLASSIC_LS_CENTER, NWII_CLASSIC_LS_RANGE, 63);
    const uint8_t rx = _nwii_scale_axis(c->rs_x, NWII_CLASSIC_RS_CENTER, NWII_CLASSIC_RS_RANGE, 31);
    const uint8_t ry = _nwii_scale_axis(c->rs_y, NWII_CLASSIC_RS_CENTER, NWII_CLASSIC_RS_RANGE, 31);

    uint8_t lt = (uint8_t)(c->lt >> 7);
    uint8_t rt = (uint8_t)(c->rt >> 7);
    if (c->l) lt = NWII_CLASSIC_TRIGGER_MAX;
    if (c->r) rt = NWII_CLASSIC_TRIGGER_MAX;

    out[0] = (uint8_t)(((rx & 0x18u) << 3) | (lx & 0x3Fu));
    out[1] = (uint8_t)(((rx & 0x06u) << 5) | (ly & 0x3Fu));
    out[2] = (uint8_t)(((rx & 0x01u) << 7) | ((lt & 0x18u) << 2) | (ry & 0x1Fu));
    out[3] = (uint8_t)(((lt & 0x07u) << 5) | (rt & 0x1Fu));

    // Buttons are active low; bit 0 of byte 4 always reads 1
    uint8_t b4 = 0x01u;
    if (c->r)     b4 |= 0x02u;
    if (c->plus)  b4 |= 0x04u;
    if (c->home)  b4 |= 0x08u;
    if (c->minus) b4 |= 0x10u;
    if (c->l)     b4 |= 0x20u;
    if (c->down)  b4 |= 0x40u;
    if (c->right) b4 |= 0x80u;

    uint8_t b5 = 0x00u;
    if (c->up)    b5 |= 0x01u;
    if (c->left)  b5 |= 0x02u;
    if (c->zr)    b5 |= 0x04u;
    if (c->x)     b5 |= 0x08u;
    if (c->a)     b5 |= 0x10u;
    if (c->y)     b5 |= 0x20u;
    if (c->b)     b5 |= 0x40u;
    if (c->zl)    b5 |= 0x80u;

    out[4] = (uint8_t)~(b4 & 0xFEu);
    out[5] = (uint8_t)~b5;
}

void nwii_extension_encode(nwii_extension_t extension, const nwii_input_s *in, uint8_t out[NWII_EXTENSION_DATA_LEN])
{
    if (in == NULL || out == NULL)
    {
        return;
    }

    switch (extension)
    {
    case NWII_EXTENSION_NUNCHUK:
        _nwii_encode_nunchuk(&in->nunchuk, out);
        break;

    case NWII_EXTENSION_CLASSIC:
    case NWII_EXTENSION_CLASSIC_PRO:
        _nwii_encode_classic(&in->classic, out);
        break;

    default:
        memset(out, 0xFF, NWII_EXTENSION_DATA_LEN);
        break;
    }
}
