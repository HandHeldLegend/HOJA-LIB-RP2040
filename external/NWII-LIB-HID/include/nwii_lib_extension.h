/**
 * @file nwii_lib_extension.h
 * @brief Extension controller register images (identification + calibration) and data encoding
 *        for the Nunchuk and Classic Controller.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef NWII_LIB_EXTENSION_H
#define NWII_LIB_EXTENSION_H

#include <stdint.h>
#include <stdbool.h>

#include "nwii_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Size of the extension register space (0xA400xx). */
#define NWII_EXTENSION_REG_SIZE     256u

/** @brief Bytes of controller data at the start of the register space. */
#define NWII_EXTENSION_DATA_LEN     6u

/** @brief Register offsets used during the host's extension handshake. */
#define NWII_EXTENSION_REG_CAL      0x20u
#define NWII_EXTENSION_REG_KEY      0x40u
#define NWII_EXTENSION_REG_CRYPT    0xF0u
#define NWII_EXTENSION_REG_ID       0xFAu

/** @brief Value written to NWII_EXTENSION_REG_CRYPT to enable/disable encryption. */
#define NWII_EXTENSION_CRYPT_ON     0xAAu
#define NWII_EXTENSION_CRYPT_OFF    0x55u

/**
 * @brief Reset an extension register image to its power-on state.
 *
 * Fills identification bytes (0xFA..0xFF) and calibration (0x20..0x3F). With
 * NWII_EXTENSION_NONE the image reads back as 0xFF, like an empty port.
 *
 * @param reg Register image of NWII_EXTENSION_REG_SIZE bytes.
 * @param extension Extension to emulate.
 */
void nwii_extension_reset_registers(uint8_t reg[NWII_EXTENSION_REG_SIZE], nwii_extension_t extension);

/**
 * @brief Encode the extension's 6 data bytes (unencrypted) for the given input.
 *
 * @param extension Attached extension.
 * @param in Input state from the firmware.
 * @param out NWII_EXTENSION_DATA_LEN bytes.
 */
void nwii_extension_encode(nwii_extension_t extension, const nwii_input_s *in, uint8_t out[NWII_EXTENSION_DATA_LEN]);

/**
 * @brief Convert milli-g to a 10-bit accelerometer sample around a calibration.
 *
 * @param mg Acceleration in milli-g.
 * @param zero 10-bit reading at 0 g.
 * @param one_g 10-bit reading at +1 g.
 * @return 0..1023.
 */
uint16_t nwii_extension_accel_to_10bit(int16_t mg, uint16_t zero, uint16_t one_g);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_EXTENSION_H */
