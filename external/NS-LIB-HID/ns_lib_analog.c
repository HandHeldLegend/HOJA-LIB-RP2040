/**
 * @file ns_lib_analog.c
 * @brief Analog calibration encode/decode for SPI factory ranges (NS-LIB-HID).
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

#include "ns_lib_analog.h"

static uint8_t s_ns_analog_calibration[NS_ANALOG_CALIBRATION_BYTE_COUNT];

/** Decode three SPI bytes to 12-bit lower/upper samples (private to this file). */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused)) /* reserved for tooling/tests that mirror SPI packing */
#endif
static void _ns_analog_decode(uint8_t s0, uint8_t s1, uint8_t s2, uint16_t *out_lower, uint16_t *out_upper)
{
    out_lower[0] = (uint16_t)(((uint16_t)(s1 << 8) & 0xF00u) | (uint16_t)s0);
    out_upper[0] = (uint16_t)(((uint16_t)s2 << 4) | ((uint16_t)s1 >> 4));
}

/** Encode one 12-bit XY pair into three bytes for SPI calibration storage. */
static void _ns_analog_encode_spi_calibration_xy12(uint16_t x, uint16_t y, uint8_t *out3)
{
    uint16_t x12 = (uint16_t)(x & 0x0FFFu);
    uint16_t y12 = (uint16_t)(y & 0x0FFFu);
    out3[0] = (uint8_t)(x12 & 0xFFu);
    out3[1] = (uint8_t)(((x12 & 0xF00u) >> 8) | ((y12 & 0x0Fu) << 4));
    out3[2] = (uint8_t)((y12 & 0xFF0u) >> 4);
}

void ns_analog_pack_xy12(uint16_t x, uint16_t y, uint8_t *out3)
{
    if (!out3)
    {
        return;
    }
    _ns_analog_encode_spi_calibration_xy12(x, y, out3);
}

void ns_analog_calibration_init(void)
{
    uint16_t min = (uint16_t)(128u << 4);
    uint16_t center = (uint16_t)(128u << 4);
    uint16_t max = (uint16_t)(128u << 4);

    _ns_analog_encode_spi_calibration_xy12(max, max, &s_ns_analog_calibration[0]);
    _ns_analog_encode_spi_calibration_xy12(center, center, &s_ns_analog_calibration[3]);
    _ns_analog_encode_spi_calibration_xy12(min, min, &s_ns_analog_calibration[6]);

    _ns_analog_encode_spi_calibration_xy12(center, center, &s_ns_analog_calibration[9]);
    _ns_analog_encode_spi_calibration_xy12(min, min, &s_ns_analog_calibration[12]);
    _ns_analog_encode_spi_calibration_xy12(max, max, &s_ns_analog_calibration[15]);
}

uint8_t *ns_analog_calibration_data(void)
{
    return s_ns_analog_calibration;
}

