/**
 * @file ns_lib_spi.h
 * @brief Emulated SPI flash map reads (migrated from HOJA switch_spi).
 *
 * Provides byte-for-byte emulation of the controller SPI flash regions the console reads over
 * the Switch HID SPI subcommand. Values are derived from @ref ns_device_config_s (via
 * ns_device_config_get) where applicable—factory controller type, colors, SNES region, etc.
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

#ifndef NS_LIB_SPI_H
#define NS_LIB_SPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Byte index where the 5-byte SPI read descriptor is written: low address, offset, 0, 0, length. */
#define NS_SPI_STARTREAD_IDX 14

/** First byte index where emulated flash contents are copied (one byte per index i). */
#define NS_SPI_READ_OUTPUT_IDX 19

/**
 * @brief Fill a HID report buffer with an emulated SPI flash read.
 *
 * Writes five bytes to @p out [@ref NS_SPI_STARTREAD_IDX] … @p out [@ref NS_SPI_STARTREAD_IDX + 4]:
 * @c { address, offset_address, 0, 0, length }. Then for each @c i in @c [0, length) sets
 * @p out [@ref NS_SPI_READ_OUTPUT_IDX + i] from the internal flash map (see @ref NS_SPI_BUFFER_LAYOUT).
 *
 * @param offset_address High byte of the SPI address (e.g. @c 0x60 for 0x60xx factory data).
 * @param address        Low byte of the starting SPI address (e.g. @c 0x12 for factory type high byte).
 * @param length         Number of bytes to return; the emulated low address is @p address + i (wraps at 256).
 * @param out            Reply buffer.
 */
void ns_spi_get(uint8_t offset_address, uint8_t address, uint8_t length, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_SPI_H */
