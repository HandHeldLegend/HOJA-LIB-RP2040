/**
 * @file nwii_lib_types.h
 * @brief Shared types for NWII-LIB-HID: device configuration, input state, extensions, IR points.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo, Wii and
 * related marks are trademarks of their respective owners.
 */

#ifndef NWII_LIB_TYPES_H
#define NWII_LIB_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Largest input report the library generates, including the report id byte. */
#define NWII_INPUT_REPORT_MAX   22u

/** @brief Largest output report the library accepts, including the report id byte. */
#define NWII_OUTPUT_REPORT_MAX  22u

/** @brief Centre value for 12-bit stick axes (0..4095). */
#define NWII_STICK_CENTER       2048

/** @brief Number of IR points the camera reports. */
#define NWII_IR_POINT_COUNT     4u

/** @brief IR camera resolution. */
#define NWII_IR_RES_X           1024u
#define NWII_IR_RES_Y           768u

/** @brief Accelerometer value that represents 1 g in nwii_input_s (milli-g). */
#define NWII_ACCEL_1G_MG        1000

/** @brief Extension controller plugged into the emulated Wii Remote. */
typedef enum
{
    NWII_EXTENSION_NONE = 0,
    NWII_EXTENSION_NUNCHUK,
    NWII_EXTENSION_CLASSIC,     ///< Original Classic Controller
    NWII_EXTENSION_CLASSIC_PRO, ///< Classic Controller Pro (same data format, different ID byte)
    NWII_EXTENSION_MAX,
} nwii_extension_t;

/** @brief One-time device configuration passed to nwii_api_init(). */
typedef struct
{
    nwii_extension_t extension; ///< Extension reported once the host connects
} nwii_device_config_s;

/**
 * @brief One IR camera point.
 *
 * x is 0..1023 and y is 0..767 in raw camera space. Set visible to false for points the camera
 * cannot see; they are reported as 0xFF bytes. nwii_ir_set_pointer() fills these from a
 * normalized cursor position.
 */
typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t  size; ///< 0..15, as reported in extended mode
    bool     visible;
} nwii_ir_point_s;

/** @brief Nunchuk state. */
typedef struct
{
    bool     c;
    bool     z;
    uint16_t stick_x; ///< 0..4095, 2048 centre, + right
    uint16_t stick_y; ///< 0..4095, 2048 centre, + up
    int16_t  accel_x; ///< milli-g
    int16_t  accel_y; ///< milli-g
    int16_t  accel_z; ///< milli-g
} nwii_nunchuk_s;

/** @brief Classic Controller / Classic Controller Pro state. */
typedef struct
{
    bool a, b, x, y;
    bool l, r;      ///< Digital L/R click (the original Classic clicks at the end of the analog travel)
    bool zl, zr;
    bool plus, minus, home;
    bool up, down, left, right;
    uint16_t ls_x;  ///< 0..4095, 2048 centre, + right
    uint16_t ls_y;  ///< 0..4095, 2048 centre, + up
    uint16_t rs_x;
    uint16_t rs_y;
    uint16_t lt;    ///< 0..4095 analog L. Reported as full scale while the L click is held.
    uint16_t rt;    ///< 0..4095 analog R
} nwii_classic_s;

/**
 * @brief Logical input state requested from the firmware for every input report.
 *
 * Accelerometer axes use the Wii Remote frame: +Z out of the button face, +Y toward the IR
 * camera (pointing direction), +X toward the remote's left side (verified axis by axis against a real remote; the frame is left-handed). At rest lying face-up the
 * remote reads (0, 0, +1000).
 */
typedef struct
{
    struct
    {
        bool a, b, one, two;
        bool plus, minus, home;
        bool up, down, left, right;
    } remote;

    int16_t accel_x; ///< milli-g
    int16_t accel_y; ///< milli-g
    int16_t accel_z; ///< milli-g

    nwii_ir_point_s ir[NWII_IR_POINT_COUNT];

    nwii_nunchuk_s nunchuk; ///< Used while the nunchuk is attached
    nwii_classic_s classic; ///< Used while a classic controller is attached
} nwii_input_s;

/** @brief Battery state for status reports. */
typedef struct
{
    uint8_t level; ///< 0..255 (0xFF = full)
    bool    low;   ///< Low-battery flag shown by the host
} nwii_power_s;

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_TYPES_H */
