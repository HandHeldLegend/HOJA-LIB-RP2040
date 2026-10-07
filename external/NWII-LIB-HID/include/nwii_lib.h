/**
 * @file nwii_lib.h
 * @brief User-facing NWII-LIB-HID API: configuration, protocol entry points and platform hooks.
 *
 * NWII-LIB-HID emulates a Wii Remote (RVL-CNT-01) with an optional Nunchuk or Classic Controller
 * extension over Bluetooth Classic HID. The library owns the Wii Remote protocol (report modes,
 * register/EEPROM access, extension identification and encryption, IR camera reports); your
 * firmware owns the Bluetooth stack and supplies input through the weak nwii_api_hook_* functions.
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

#ifndef NWII_LIB_API_H
#define NWII_LIB_API_H

#include <stdint.h>
#include <stdbool.h>

#include "nwii_lib_types.h"
#include "nwii_lib_hid.h"
#include "nwii_lib_ir.h"
#include "nwii_lib_aim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- API Functions --- */

/**
 * @brief Initialize the protocol engine. Call once before the first connection.
 *
 * @param cfg Device configuration; NULL selects defaults (no extension).
 * @return True on success.
 */
bool nwii_api_init(const nwii_device_config_s *cfg);

/**
 * @brief Reset per-connection protocol state (report mode, IR, extension handshake).
 *
 * Call when the host opens a new HID connection. The configured extension is re-announced to the
 * host shortly afterwards, matching a real remote with an extension already plugged in.
 */
void nwii_api_connection_reset(void);

/**
 * @brief Build the next input report in-place.
 *
 * Writes the report id to `data[0]` and the payload to `data[1..]`. Responses to host requests
 * (acknowledgements, status, memory reads) take priority over regular data reports.
 *
 * Output reports queued by nwii_api_output_tunnel() are processed here, so all protocol state is
 * owned by whichever context calls this function. Call it at the input report cadence (a real
 * remote reports at roughly 100 Hz).
 *
 * @param data Output buffer of NWII_INPUT_REPORT_MAX bytes.
 * @param[out] len Bytes written including the report id.
 * @return True when a report was produced.
 */
bool nwii_api_generate_inputreport(uint8_t data[NWII_INPUT_REPORT_MAX], uint8_t *len);

/**
 * @brief Feed one host output report into the library.
 *
 * `data[0]` is the report id (0x10..0x1A) followed by its payload, without the Bluetooth HID
 * transaction header (0xA2/0x52). Rumble and player LED hooks fire immediately from this call;
 * everything else is deferred to nwii_api_generate_inputreport(). The hand-off is a single
 * producer / single consumer queue, so this may be called from a different context than the
 * generator as long as each side has only one caller.
 *
 * @param data Report bytes.
 * @param len Length in bytes.
 */
void nwii_api_output_tunnel(const uint8_t *data, uint16_t len);

/**
 * @brief Change the attached extension at runtime.
 *
 * The library emulates an unplug followed by a plug-in, so the host re-identifies the new
 * extension exactly as it would on real hardware.
 *
 * @param extension New extension, or NWII_EXTENSION_NONE to unplug.
 */
void nwii_api_set_extension(nwii_extension_t extension);

/** @brief Extension most recently requested through config or nwii_api_set_extension(). */
nwii_extension_t nwii_api_get_extension(void);

/* --- API Platform Hook Functions (weak, override in firmware) --- */

/**
 * @brief Fill the logical input state for the next report.
 *
 * Called from nwii_api_generate_inputreport(). Fill the members that apply to the attached
 * extension; the rest are ignored.
 *
 * @param out Input state, zeroed with sticks centred and visible=false IR points before the call.
 */
void nwii_api_hook_get_input(nwii_input_s *out);

/**
 * @brief Host rumble request. Called from nwii_api_output_tunnel() whenever the motor bit changes.
 *
 * @param enable True to run the rumble motor.
 */
void nwii_api_hook_set_rumble(bool enable);

/**
 * @brief Host player LED request. Called from nwii_api_output_tunnel() on report 0x11.
 *
 * @param led_mask Bit 0 is LED 1 (leftmost) through bit 3 for LED 4.
 */
void nwii_api_hook_set_leds(uint8_t led_mask);

/**
 * @brief Battery state for status reports (0x20).
 *
 * @param out Power state; defaults to full battery.
 */
void nwii_api_hook_get_power(nwii_power_s *out);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_API_H */
