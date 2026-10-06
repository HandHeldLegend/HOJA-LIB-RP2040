/**
 * @file nwii_lib_protocol.h
 * @brief Wii Remote report protocol engine (internal; use nwii_lib.h).
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef NWII_LIB_PROTOCOL_H
#define NWII_LIB_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#include "nwii_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Output report ids (host -> remote) */
#define NWII_OUT_RUMBLE         0x10u
#define NWII_OUT_LEDS           0x11u
#define NWII_OUT_REPORT_MODE    0x12u
#define NWII_OUT_IR_ENABLE      0x13u
#define NWII_OUT_SPEAKER_ENABLE 0x14u
#define NWII_OUT_STATUS_REQUEST 0x15u
#define NWII_OUT_WRITE_MEMORY   0x16u
#define NWII_OUT_READ_MEMORY    0x17u
#define NWII_OUT_SPEAKER_DATA   0x18u
#define NWII_OUT_SPEAKER_MUTE   0x19u
#define NWII_OUT_IR_ENABLE_2    0x1Au

/* Input report ids (remote -> host) */
#define NWII_IN_STATUS          0x20u
#define NWII_IN_READ_DATA       0x21u
#define NWII_IN_ACK             0x22u
#define NWII_IN_MODE_DEFAULT    0x30u

void nwii_protocol_init(nwii_extension_t extension);
void nwii_protocol_connection_reset(void);
void nwii_protocol_set_extension(nwii_extension_t extension);
nwii_extension_t nwii_protocol_get_extension(void);
void nwii_protocol_ingest_outputreport(const uint8_t *data, uint16_t len);
bool nwii_protocol_generate_inputreport(uint8_t *data, uint8_t *len);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_PROTOCOL_H */
