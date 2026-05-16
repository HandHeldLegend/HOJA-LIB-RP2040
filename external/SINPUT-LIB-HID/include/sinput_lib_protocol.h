/*
 * SINPUT-LIB-HID — Protocol helpers (reports, scaling, feature payload, parsing).
 *
 * Copyright (c) 2026 Hand Held Legend, LLC
 * Author: Mitchell Cairns
 *
 * SPDX-License-Identifier: MIT-0
 */

#ifndef SINPUT_LIB_PROTOCOL_H
#define SINPUT_LIB_PROTOCOL_H

#include "sinput_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Packs face-style (3 bits) and sub-type (5 bits) into the single features byte. */
static inline uint8_t sinput_pack_subtype_facestyle(uint8_t subtype_5bit, uint8_t face_style_3bit)
{
    return (uint8_t)(((face_style_3bit & 7u) << 5) | (subtype_5bit & 0x1Fu));
}

void sinput_input_masks_from_hw(const sinput_hw_capabilities_s *hw, uint8_t masks[4]);

/** Fills sensible defaults (protocol v1, ±8G accel, 2000 dps gyro, 1 ms polling). */
void sinput_features_params_apply_defaults(sinput_features_params_s *params);

/**
 * Encodes the 24-byte features payload (written starting at host buffer index 2
 * after Report ID 0x02 and command byte 0x02).
 */
void sinput_features_payload_encode(uint8_t payload[SINPUT_FEATURES_PAYLOAD_LEN], const sinput_features_params_s *params);

void sinput_pack_input_report(uint8_t report[SINPUT_REPORT_LEN_INPUT], const sinput_input_s *input);

void sinput_pack_features_command_reply(uint8_t report[SINPUT_REPORT_LEN_INPUT],
                                        const uint8_t feature_payload[SINPUT_FEATURES_PAYLOAD_LEN]);

/**
 * Maps a 12-bit style trigger sample (0..4095) to the int16 range used on the wire.
 * Values above 4095 are clamped.
 */
int16_t sinput_scale_trigger_uint12(uint16_t val);

/** Scales a raw stick axis (typically ±2048) to the HID int16 range. */
int16_t sinput_scale_axis_int16(int16_t input_axis);

typedef struct
{
    uint8_t command;
    const uint8_t *payload;
    size_t payload_len;
} sinput_parsed_output_s;

/**
 * Parses a host output buffer. If the first byte matches Report ID 0x03, the
 * command byte is taken from index 1 and the payload from index 2 onward
 * (TinyUSB / HOJA convention). Otherwise the command is at index 0 (payload at 1).
 */
bool sinput_try_parse_output_report(const uint8_t *buffer, uint16_t len, sinput_parsed_output_s *out);

/**
 * Copies a haptic command payload into @p out when large enough for @p out's layout.
 * Returns false if @p payload is NULL or shorter than @ref sinput_haptic_s.
 */
bool sinput_haptic_payload_copy(const uint8_t *payload, size_t payload_len, sinput_haptic_s *out);

#ifdef __cplusplus
}
#endif

#endif /* SINPUT_LIB_PROTOCOL_H */
