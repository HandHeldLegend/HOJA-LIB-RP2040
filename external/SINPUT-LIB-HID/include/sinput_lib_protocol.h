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

bool sinput_protocol_generate_inputreport(uint8_t out[64]);

void sinput_protocol_output_tunnel(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SINPUT_LIB_PROTOCOL_H */
