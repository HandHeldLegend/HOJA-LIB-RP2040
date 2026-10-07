/**
 * @file nwii_lib_motionplus.h
 * @brief Wii MotionPlus emulation (internal): the gyro's register space, activation over an
 *        attached extension, the host's authentication challenge, and data frames.
 *
 * The emulated MotionPlus is built in, as on a Wii Remote Plus. While inactive it answers in its own
 * register space (0xA6) and the extension port behaves normally. A host write to 0xA600FE activates
 * it: the MotionPlus then takes over the extension space (0xA4), reports gyro data, and can
 * interleave ("pass through") Nunchuk or Classic Controller data. Any write to 0xA400F0 deactivates
 * it again.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef NWII_LIB_MOTIONPLUS_H
#define NWII_LIB_MOTIONPLUS_H

#include <stdint.h>
#include <stdbool.h>

#include "nwii_lib_types.h"
#include "nwii_lib_extension.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    NWII_MP_ABSENT = 0,   ///< No MotionPlus in this remote
    NWII_MP_INACTIVE,     ///< Answers at 0xA6; the extension port works normally
    NWII_MP_ACTIVATING,   ///< Briefly unresponsive while switching over
    NWII_MP_ACTIVE,       ///< Owns 0xA4 and reports gyro data
    NWII_MP_DEACTIVATING, ///< Briefly unresponsive while handing 0xA4 back
} nwii_mp_status_t;

/** @brief Reset to the power-on (inactive) state. present = false removes the MotionPlus. */
void nwii_mp_reset(bool present);

nwii_mp_status_t nwii_mp_status(void);

/** @brief State of the extension detect line the remote reports in status reports. */
bool nwii_mp_detect_pin(bool extension_present);

/** @brief Register read in whichever space the MotionPlus currently answers. */
void nwii_mp_read(uint8_t offset, uint8_t *dst, uint8_t size);

/** @brief Write to 0xA6 while inactive (0xFE selects a mode and activates). */
void nwii_mp_write_inactive(uint8_t offset, const uint8_t *src, uint8_t size);

/**
 * @brief Write to 0xA4 while active.
 *
 * @return true when the write deactivated the MotionPlus through 0xF0. The same write then also
 *         reaches the extension behind it (it doubles as the extension's init).
 */
bool nwii_mp_write_active(uint8_t offset, const uint8_t *src, uint8_t size);

/**
 * @brief Advance timers and the challenge; call once per input report.
 *
 * @param extension Extension plugged in behind the MotionPlus.
 * @param ext_reg That extension's register image (identification and calibration are mirrored).
 */
void nwii_mp_step(nwii_extension_t extension, const uint8_t ext_reg[NWII_EXTENSION_REG_SIZE]);

/**
 * @brief Build the next 6-byte data frame while active: gyro data, or pass-through extension data
 *        alternating with gyro data when the host selected a pass-through mode.
 */
void nwii_mp_build(uint8_t out[NWII_EXTENSION_DATA_LEN], const nwii_input_s *in, nwii_extension_t extension);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_MOTIONPLUS_H */
