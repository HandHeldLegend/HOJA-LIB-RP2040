/**
 * @file ns_lib_analog.h
 * @brief 12-bit packed stick calibration blob exposed for SPI factory emulation (`ns_lib_spi.c`).
 *
 * SPI tuple packing helpers are static inside `ns_lib_analog.c` (not part of this header).
 *
 * Copyright (c) 2026 Mitchell Cairns, Hand Held Legend, LLC.
 *
 * Licensed under the Creative Commons Attribution-NonCommercial 4.0 International License
 * (CC BY-NC 4.0). Non-commercial use with attribution; commercial licensing from Hand Held Legend.
 * Full terms: https://creativecommons.org/licenses/by-nc/4.0/legalcode — see LICENSING.md in this folder
 *
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo and
 * related marks are trademarks of their respective owners.
 */

#ifndef NS_LIB_ANALOG_H
#define NS_LIB_ANALOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Length of the factory stick calibration blob (left + right SPI ranges). */
#define NS_ANALOG_CALIBRATION_BYTE_COUNT 18

/**
 * @brief Fill default centered calibration samples into the internal buffer.
 *
 * Call during startup (e.g. from @ref ns_api_init); safe to call again to reset defaults.
 */
void ns_analog_calibration_init(void);

/**
 * @brief Pack one 12-bit stick XY pair into Switch report nibble layout.
 *
 * Byte layout mirrors standard report fields (`out->data[6..8]` / `out->data[9..11]`):
 * - out[0] = x[7:0]
 * - out[1] = x[11:8] | y[3:0] << 4
 * - out[2] = y[11:4]
 *
 * Inputs are masked to 12 bits.
 *
 * @param x 12-bit X sample.
 * @param y 12-bit Y sample.
 * @param out3 Destination 3-byte buffer.
 */
void ns_analog_pack_xy12(uint16_t x, uint16_t y, uint8_t *out3);

/**
 * @brief Mutable pointer to the calibration blob used for SPI factory stick ranges.
 *
 * Lifetime is static inside `ns_lib_analog.c`. The pointed-to array has length
 * @ref NS_ANALOG_CALIBRATION_BYTE_COUNT (three 3-byte encoded tuples per stick: left then right).
 *
 * Call @ref ns_analog_calibration_init before SPI reads if defaults are required.
 */
uint8_t *ns_analog_calibration_data(void);

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_ANALOG_H */
