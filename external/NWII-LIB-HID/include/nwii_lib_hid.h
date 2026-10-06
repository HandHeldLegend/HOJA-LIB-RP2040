/**
 * @file nwii_lib_hid.h
 * @brief Bluetooth identity for Wii Remote emulation: name, class of device, HID descriptor, SDP
 *        record and pairing PIN.
 *
 * The Wii discovers remotes by name and class of device, checks the HID service record, and pairs
 * with legacy PIN pairing (Secure Simple Pairing must be disabled on the local controller).
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

#ifndef NWII_LIB_HID_H
#define NWII_LIB_HID_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Class of device advertised by a Wii Remote (peripheral, joystick). */
#define NWII_HID_CLASS_OF_DEVICE        0x002504u

/** @brief Vendor and product id of the RVL-CNT-01. */
#define NWII_HID_VID                    0x057Eu
#define NWII_HID_PID                    0x0306u

/** @brief Byte length of the HID report descriptor blob. */
#define NWII_HID_REPORT_DESCRIPTOR_LEN  217u

/** @brief Byte length of the SDP HID service record blob. */
#define NWII_HID_SDP_RECORD_LEN         463u

/** @brief Service record handle embedded in the SDP record blob. */
#define NWII_HID_SDP_RECORD_HANDLE      0x00010000u

/** @brief Length of the binary pairing PIN. */
#define NWII_HID_PIN_LEN                6u

/**
 * @brief Bluetooth device name the Wii searches for.
 *
 * @return "Nintendo RVL-CNT-01"; do not modify.
 */
const char *nwii_hid_get_device_name(void);

/**
 * @brief HID report descriptor of the RVL-CNT-01.
 *
 * @param[out] descriptor ROM descriptor bytes; may be NULL.
 * @param[out] len Descriptor length; may be NULL.
 */
void nwii_hid_get_report_descriptor(const uint8_t **descriptor, uint16_t *len);

/**
 * @brief Complete SDP service record (data element sequence) for the HID service.
 *
 * Matches the record of a real remote, including the HID descriptor and the attributes the Wii
 * verifies. The record carries NWII_HID_SDP_RECORD_HANDLE as its ServiceRecordHandle; register it
 * before allocating other record handles so they do not collide.
 *
 * @param[out] record ROM record bytes; may be NULL.
 * @param[out] len Record length; may be NULL.
 */
void nwii_hid_get_sdp_record(const uint8_t **record, uint16_t *len);

/**
 * @brief Binary PIN for legacy pairing with a Wii.
 *
 * When the Wii's red SYNC button is used, the PIN is the Wii's own Bluetooth address with the
 * bytes in reverse order. Answer the HCI PIN Code Request with these 6 bytes.
 *
 * @param host_addr Wii address in display order (most significant byte first).
 * @param[out] pin_out NWII_HID_PIN_LEN bytes.
 */
void nwii_hid_make_pin(const uint8_t host_addr[6], uint8_t pin_out[NWII_HID_PIN_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_HID_H */
