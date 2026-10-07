/**
 * @file nwii_lib_motionplus.c
 * @brief Wii MotionPlus emulation: registers, activation, authentication challenge, data frames.
 *
 * Protocol behaviour follows the public documentation of the MotionPlus (WiiBrew) and the observed
 * behaviour documented by the Dolphin project. This is an independent implementation.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#include "nwii_lib_motionplus.h"

#include <math.h>
#include <string.h>

/* Register offsets */
#define NWII_MP_REG_CALIBRATION     0x20u
#define NWII_MP_REG_PASS_CALIB      0x40u
#define NWII_MP_REG_CHALLENGE       0x50u
#define NWII_MP_REG_INIT            0xF0u
#define NWII_MP_REG_CHALLENGE_TYPE  0xF1u
#define NWII_MP_REG_PASS_ID4        0xF6u
#define NWII_MP_REG_CHALLENGE_STATE 0xF7u
#define NWII_MP_REG_PASS_ID0        0xF8u
#define NWII_MP_REG_PASS_ID5        0xF9u
#define NWII_MP_REG_ID              0xFAu
#define NWII_MP_REG_ID_SPACE        0xFCu // Identifier byte 2: which address space answers
#define NWII_MP_REG_MODE            0xFEu // Identifier byte 4: pass-through mode

#define NWII_MP_SPACE_INACTIVE      0xA6u
#define NWII_MP_SPACE_ACTIVE        0xA4u

/* Pass-through modes the host writes to 0xFE */
#define NWII_MP_MODE_ALONE          0x04u
#define NWII_MP_MODE_NUNCHUK        0x05u
#define NWII_MP_MODE_CLASSIC        0x07u

/* Challenge progress at 0xF7. The host reads parameter x once it shows X_READY, asks for y0 or y1
 * through 0xF1, then reads y once it shows Y_READY. */
#define NWII_MP_CHALLENGE_ACTIVATING 0x00u // Not a real value: marks the unresponsive switch-over
#define NWII_MP_CHALLENGE_PREP_X     0x02u
#define NWII_MP_CHALLENGE_X_READY    0x0Eu
#define NWII_MP_CHALLENGE_PREP_Y     0x14u
#define NWII_MP_CHALLENGE_Y_READY    0x1Au

/* Timings in input reports (~10 ms each). Switching over takes ~20 ms on real hardware. Parameter x
 * must not be ready instantly: the host's SDK only waits correctly if it first sees it pending. */
#define NWII_MP_SWITCH_REPORTS      3u
#define NWII_MP_PREP_X_REPORTS      50u
#define NWII_MP_PREP_Y0_REPORTS     3u
#define NWII_MP_PREP_Y1_REPORTS     50u

/* Gyro encoding: 14-bit values centred on 8192. Slow (precise) mode covers about +/-500 deg/s; fast
 * mode the rest. Counts per deg/s follow from the calibration below (0x4400 / 4 counts per its
 * full-scale degrees). */
#define NWII_MP_ZERO                8192
#define NWII_MP_MAX                 16383
#define NWII_MP_SLOW_COUNTS_PER_DPS (4352.0f / 270.0f)
#define NWII_MP_FAST_COUNTS_PER_DPS (4352.0f / 1200.0f)
#define NWII_MP_SLOW_LIMIT_DPS      500.0f

/* Built in, as on a Wii Remote Plus (first identifier byte 0x01). */
static const uint8_t _nwii_mp_id_inactive[6] = {0x01, 0x00, NWII_MP_SPACE_INACTIVE, 0x20, 0x00, 0x05};

/* Calibration (fast block, uid, CRC high half, slow block, uid, CRC low half). Zero at 0x8000,
 * scale 0x8000 -/+ 0x4400 for 1200 deg/s (fast) and 270 deg/s (slow), with yaw and pitch scales
 * negative like a real MotionPlus. The CRC is CRC-32 over bytes 0x00..0x0D and 0x10..0x1D. */
static const uint8_t _nwii_mp_calibration[32] = {
    0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x3C, 0x00, 0xC4, 0x00, 0x3C, 0x00, 0xC8, 0x4E, 0xEA, 0xEA,
    0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x3C, 0x00, 0xC4, 0x00, 0x3C, 0x00, 0x2D, 0x57, 0x92, 0xBC,
};

/*
 * Authentication challenge: a Fiat-Shamir style proof over the published MotionPlus modulus n and
 * key. For a chosen r the MotionPlus presents x = r^2 mod n, then answers y0 = r or
 * y1 = r * sqrt(v) mod n. These are precomputed for r = "NWII-LIB-HID: free for everyone." (big
 * endian) and stored little endian, as the host reads them.
 */
static const uint8_t _nwii_mp_param_x[64] = {
    0x44, 0x54, 0x85, 0xFF, 0xD4, 0x88, 0x45, 0x17, 0x87, 0x69, 0x1B, 0x01, 0x21, 0x78, 0xC5, 0xC9,
    0xF9, 0x53, 0xCA, 0x10, 0xA2, 0x68, 0xB1, 0x53, 0xDC, 0xFF, 0xF0, 0x2F, 0x04, 0x8B, 0xC1, 0xA0,
    0x42, 0x0F, 0x51, 0x07, 0x06, 0x01, 0x21, 0xF4, 0x1C, 0x15, 0x2C, 0xB8, 0x4C, 0x4B, 0x75, 0x10,
    0x1F, 0x0E, 0xFB, 0x44, 0xD5, 0x41, 0xE0, 0x7D, 0x3E, 0x17, 0x52, 0x7C, 0x6B, 0x4E, 0xF9, 0x17,
};
static const uint8_t _nwii_mp_param_y0[64] = {
    0x2E, 0x65, 0x6E, 0x6F, 0x79, 0x72, 0x65, 0x76, 0x65, 0x20, 0x72, 0x6F, 0x66, 0x20, 0x65, 0x65,
    0x72, 0x66, 0x20, 0x3A, 0x44, 0x49, 0x48, 0x2D, 0x42, 0x49, 0x4C, 0x2D, 0x49, 0x49, 0x57, 0x4E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t _nwii_mp_param_y1[64] = {
    0xD4, 0x99, 0xF8, 0x9D, 0x2A, 0x80, 0xCC, 0x72, 0xAF, 0x61, 0xC4, 0xBA, 0x19, 0x72, 0x13, 0x1C,
    0x9D, 0xD0, 0xF5, 0x85, 0xD7, 0xD1, 0xFC, 0x4B, 0xEE, 0xED, 0x56, 0xCC, 0xAD, 0x02, 0xF6, 0x18,
    0x7F, 0x39, 0x8C, 0x5E, 0xB6, 0x9C, 0x25, 0x94, 0x81, 0xA3, 0xEF, 0xC3, 0xBA, 0x55, 0x18, 0xCD,
    0x40, 0x2A, 0xC3, 0x59, 0x35, 0xC5, 0x26, 0x06, 0x12, 0x2D, 0xFE, 0x98, 0xF8, 0x46, 0xDD, 0x07,
};

typedef struct
{
    bool    present;
    uint8_t reg[256];
    uint8_t timer;         // Reports until the current switch-over or challenge step completes
    bool    ext_connected; // Extension behind the MotionPlus, as last reported to the host
    bool    last_was_gyro; // Pass-through alternates gyro and extension frames
} nwii_mp_state_s;

static nwii_mp_state_s _mp;

static inline bool _nwii_mp_mode_valid(uint8_t mode)
{
    return (mode == NWII_MP_MODE_ALONE) || (mode == NWII_MP_MODE_NUNCHUK) || (mode == NWII_MP_MODE_CLASSIC);
}

static inline bool _nwii_mp_covers(uint8_t offset, uint8_t size, uint8_t reg)
{
    return (reg >= offset) && ((uint16_t)reg < (uint16_t)offset + size);
}

void nwii_mp_reset(bool present)
{
    memset(&_mp, 0, sizeof(_mp));
    _mp.present = present;

    memcpy(&_mp.reg[NWII_MP_REG_CALIBRATION], _nwii_mp_calibration, sizeof(_nwii_mp_calibration));
    memcpy(&_mp.reg[NWII_MP_REG_ID], _nwii_mp_id_inactive, sizeof(_nwii_mp_id_inactive));
}

nwii_mp_status_t nwii_mp_status(void)
{
    if (!_mp.present)
        return NWII_MP_ABSENT;

    if (_mp.reg[NWII_MP_REG_ID_SPACE] == NWII_MP_SPACE_ACTIVE)
        return (_mp.reg[NWII_MP_REG_CHALLENGE_STATE] == NWII_MP_CHALLENGE_ACTIVATING) ? NWII_MP_ACTIVATING
                                                                                       : NWII_MP_ACTIVE;

    return (_mp.timer != 0u) ? NWII_MP_DEACTIVATING : NWII_MP_INACTIVE;
}

bool nwii_mp_detect_pin(bool extension_present)
{
    switch (nwii_mp_status())
    {
    case NWII_MP_ACTIVE:
        return true;

    case NWII_MP_ACTIVATING:
    case NWII_MP_DEACTIVATING:
        // The port goes quiet while switching over, so the host sees an unplug and a plug-in
        return false;

    case NWII_MP_ABSENT:
    case NWII_MP_INACTIVE:
    default:
        return extension_present;
    }
}

void nwii_mp_read(uint8_t offset, uint8_t *dst, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
        dst[i] = _mp.reg[(uint8_t)(offset + i)];
}

static void _nwii_mp_activate(void)
{
    _mp.reg[NWII_MP_REG_ID_SPACE] = NWII_MP_SPACE_ACTIVE;
    memset(_mp.reg, 0, NWII_EXTENSION_DATA_LEN);
    _mp.reg[NWII_MP_REG_CHALLENGE_STATE] = NWII_MP_CHALLENGE_ACTIVATING;
    _mp.ext_connected = false; // Re-announced (with its ID and calibration) once active
    _mp.last_was_gyro = false;
    _mp.timer = NWII_MP_SWITCH_REPORTS;
}

static void _nwii_mp_deactivate(void)
{
    _mp.reg[NWII_MP_REG_ID_SPACE] = NWII_MP_SPACE_INACTIVE;
    _mp.timer = NWII_MP_SWITCH_REPORTS;
}

void nwii_mp_write_inactive(uint8_t offset, const uint8_t *src, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
        _mp.reg[(uint8_t)(offset + i)] = src[i];

    if (_nwii_mp_covers(offset, size, NWII_MP_REG_MODE) && _nwii_mp_mode_valid(_mp.reg[NWII_MP_REG_MODE]))
        _nwii_mp_activate();
}

bool nwii_mp_write_active(uint8_t offset, const uint8_t *src, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
        _mp.reg[(uint8_t)(offset + i)] = src[i];

    // Any value at 0xF0 hands the port back to the extension
    if (_nwii_mp_covers(offset, size, NWII_MP_REG_INIT))
    {
        _nwii_mp_deactivate();
        return true;
    }

    if (_nwii_mp_covers(offset, size, NWII_MP_REG_CHALLENGE_TYPE) &&
        (_mp.reg[NWII_MP_REG_CHALLENGE_STATE] == NWII_MP_CHALLENGE_X_READY))
    {
        // 0 asks for y0 (quick), 1 for y1 (slow on real hardware)
        _mp.timer = (_mp.reg[NWII_MP_REG_CHALLENGE_TYPE] == 0u) ? NWII_MP_PREP_Y0_REPORTS : NWII_MP_PREP_Y1_REPORTS;
        _mp.reg[NWII_MP_REG_CHALLENGE_STATE] = NWII_MP_CHALLENGE_PREP_Y;
    }

    // Selecting another pass-through mode switches over; anything else (e.g. 0) deactivates
    if (_nwii_mp_covers(offset, size, NWII_MP_REG_MODE) && !_nwii_mp_mode_valid(_mp.reg[NWII_MP_REG_MODE]))
        _nwii_mp_deactivate();

    return false;
}

void nwii_mp_step(nwii_extension_t extension, const uint8_t ext_reg[NWII_EXTENSION_REG_SIZE])
{
    if (!_mp.present)
        return;

    if (_mp.timer)
        _mp.timer--;

    if (!_mp.timer && (nwii_mp_status() == NWII_MP_ACTIVATING))
    {
        _mp.reg[NWII_MP_REG_CHALLENGE_STATE] = NWII_MP_CHALLENGE_PREP_X;
        _mp.timer = NWII_MP_PREP_X_REPORTS;
    }

    if (nwii_mp_status() != NWII_MP_ACTIVE)
        return;

    // A newly connected extension is mirrored into the MotionPlus registers for the host
    const bool ext_present = (extension != NWII_EXTENSION_NONE);
    if (ext_present != _mp.ext_connected)
    {
        if (ext_present)
        {
            _mp.reg[NWII_MP_REG_PASS_ID0] = ext_reg[NWII_EXTENSION_REG_ID + 0u];
            _mp.reg[NWII_MP_REG_PASS_ID4] = ext_reg[NWII_EXTENSION_REG_ID + 4u];
            _mp.reg[NWII_MP_REG_PASS_ID5] = ext_reg[NWII_EXTENSION_REG_ID + 5u];
            memcpy(&_mp.reg[NWII_MP_REG_PASS_CALIB], &ext_reg[NWII_EXTENSION_REG_CAL], 16);
        }
        _mp.ext_connected = ext_present;
    }

    if (_mp.timer)
        return;

    switch (_mp.reg[NWII_MP_REG_CHALLENGE_STATE])
    {
    case NWII_MP_CHALLENGE_PREP_X:
        memcpy(&_mp.reg[NWII_MP_REG_CHALLENGE], _nwii_mp_param_x, 64);
        _mp.reg[NWII_MP_REG_CHALLENGE_STATE] = NWII_MP_CHALLENGE_X_READY;
        break;

    case NWII_MP_CHALLENGE_PREP_Y:
        memcpy(&_mp.reg[NWII_MP_REG_CHALLENGE],
               (_mp.reg[NWII_MP_REG_CHALLENGE_TYPE] == 0u) ? _nwii_mp_param_y0 : _nwii_mp_param_y1, 64);
        _mp.reg[NWII_MP_REG_CHALLENGE_STATE] = NWII_MP_CHALLENGE_Y_READY;
        break;

    default:
        break;
    }
}

static uint16_t _nwii_mp_axis(float dps, bool *slow)
{
    *slow = fabsf(dps) < NWII_MP_SLOW_LIMIT_DPS;
    const float counts = dps * (*slow ? NWII_MP_SLOW_COUNTS_PER_DPS : NWII_MP_FAST_COUNTS_PER_DPS);
    int32_t value = NWII_MP_ZERO + (int32_t)lroundf(counts);
    if (value < 0) value = 0;
    if (value > NWII_MP_MAX) value = NWII_MP_MAX;
    return (uint16_t)value;
}

static void _nwii_mp_gyro_frame(uint8_t out[NWII_EXTENSION_DATA_LEN], const nwii_input_s *in)
{
    bool yaw_slow, roll_slow, pitch_slow;
    const uint16_t yaw = _nwii_mp_axis(in->gyro_dps.yaw, &yaw_slow);
    const uint16_t roll = _nwii_mp_axis(in->gyro_dps.roll, &roll_slow);
    const uint16_t pitch = _nwii_mp_axis(in->gyro_dps.pitch, &pitch_slow);

    out[0] = (uint8_t)yaw;
    out[1] = (uint8_t)roll;
    out[2] = (uint8_t)pitch;
    out[3] = (uint8_t)(((yaw >> 8) << 2) | (yaw_slow ? 0x02u : 0u) | (pitch_slow ? 0x01u : 0u));
    out[4] = (uint8_t)(((roll >> 8) << 2) | (roll_slow ? 0x02u : 0u));
    out[5] = (uint8_t)(((pitch >> 8) << 2) | 0x02u); // Bit 1 marks a gyro frame
}

/* Extension data squeezed through the MotionPlus: the low bit of some values gives way to the
 * frame flags (byte 4 bit 0, byte 5 bits 0..1). */
static void _nwii_mp_passthrough_frame(uint8_t out[NWII_EXTENSION_DATA_LEN], const nwii_input_s *in,
                                       nwii_extension_t extension, uint8_t mode)
{
    uint8_t raw[NWII_EXTENSION_DATA_LEN];
    nwii_extension_encode(extension, in, raw);
    memcpy(out, raw, NWII_EXTENSION_DATA_LEN);

    if (mode == NWII_MP_MODE_NUNCHUK)
    {
        // Byte 4 keeps accel Z bits 9..3; byte 5 becomes Z<2>, Z<1>, Y<1>, X<1>, C, Z, 0, 0
        out[5] = (uint8_t)(((raw[4] & 0x01u) << 7) |  // Accel Z bit 2
                           ((raw[5] & 0x80u) >> 1) |  // Accel Z bit 1
                           (raw[5] & 0x20u) |         // Accel Y bit 1
                           ((raw[5] & 0x08u) << 1) |  // Accel X bit 1
                           ((raw[5] & 0x02u) << 2) |  // C
                           ((raw[5] & 0x01u) << 2));  // Z
    }
    else
    {
        // Classic: D-pad up and left move into bit 0 of bytes 0 and 1 (the left stick's low bits)
        out[0] = (uint8_t)((raw[0] & 0xFEu) | (raw[5] & 0x01u));
        out[1] = (uint8_t)((raw[1] & 0xFEu) | ((raw[5] >> 1) & 0x01u));
        out[5] = (uint8_t)(raw[5] & 0xFCu);
    }

    out[4] &= 0xFEu;
}

void nwii_mp_build(uint8_t out[NWII_EXTENSION_DATA_LEN], const nwii_input_s *in, nwii_extension_t extension)
{
    const uint8_t mode = _mp.reg[NWII_MP_REG_MODE];
    const bool can_pass = (mode != NWII_MP_MODE_ALONE) && _mp.ext_connected && (extension != NWII_EXTENSION_NONE);

    // Pass-through alternates frames; standalone (or nothing to pass) always sends gyro
    const bool gyro = !can_pass || !_mp.last_was_gyro;
    _mp.last_was_gyro = gyro;

    if (gyro)
        _nwii_mp_gyro_frame(out, in);
    else
        _nwii_mp_passthrough_frame(out, in, extension, mode);

    if (_mp.ext_connected)
        out[4] |= 0x01u;

    memcpy(_mp.reg, out, NWII_EXTENSION_DATA_LEN);
}
