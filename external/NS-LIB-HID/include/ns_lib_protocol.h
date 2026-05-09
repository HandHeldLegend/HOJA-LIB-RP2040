/**
 * @file ns_lib_protocol.h
 * @brief Protocol command IDs and input/command report helpers (NS-LIB-HID).
 *
 * Copyright (c) 2026 Mitchell Cairns, Hand Held Legend, LLC.
 *
 * Licensed under the Creative Commons Attribution-NonCommercial 4.0 International License
 * (CC BY-NC 4.0). Non-commercial use with attribution; commercial licensing from Hand Held Legend.
 * Full terms: https://creativecommons.org/licenses/by-nc/4.0/legalcode — see LICENSING.md in this folder
 *
 * SPDX-License-Identifier: CC-BY-NC-4.0
 */

#ifndef NS_LIB_PROTOCOL_H
#define NS_LIB_PROTOCOL_H

#include <stdint.h>

#include "ns_lib_motion.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NS_LIB_PROTOCOL_OUT_ID_RUMBLE_CMD 0x01
#define NS_LIB_PROTOCOL_OUT_ID_INFO 0x80
#define NS_LIB_PROTOCOL_OUT_ID_RUMBLE 0x10

#define NS_LIB_PROTOCOL_GET_STATE 0x00
#define NS_LIB_PROTOCOL_SET_PAIRING 0x01
#define NS_LIB_PROTOCOL_GET_DEVICEINFO 0x02
#define NS_LIB_PROTOCOL_SET_INPUTMODE 0x03
#define NS_LIB_PROTOCOL_GET_TRIGGERET 0x04
#define NS_LIB_PROTOCOL_GET_PAGELIST 0x05
#define NS_LIB_PROTOCOL_SET_HCI 0x06
#define NS_LIB_PROTOCOL_SET_SHIPMODE 0x08
#define NS_LIB_PROTOCOL_GET_SPI 0x10
#define NS_LIB_PROTOCOL_SET_SPI 0x11
#define NS_LIB_PROTOCOL_SET_NFC 0x21
#define NS_LIB_PROTOCOL_SET_NFC_STATE 0x22
#define NS_LIB_PROTOCOL_ENABLE_IMU 0x40
#define NS_LIB_PROTOCOL_SET_IMUSENS 0x41
#define NS_LIB_PROTOCOL_ENABLE_VIBRATE 0x48
#define NS_LIB_PROTOCOL_SET_PLAYER 0x30
#define NS_LIB_PROTOCOL_GET_PLAYER 0x31
#define NS_LIB_PROTOCOL_33 0x33

extern uint8_t ns_lib_protocol_in_command_buffer[64];
extern uint8_t ns_lib_protocol_in_report_id;
extern uint16_t ns_lib_protocol_in_command_len;
extern uint8_t ns_lib_protocol_in_command_got;

extern uint8_t ns_lib_protocol_reporting_mode;
extern uint8_t ns_lib_protocol_imu_mode;

extern uint8_t ns_lib_protocol_command_buffer[64];
extern uint8_t ns_lib_protocol_command_report_id;

extern uint8_t ns_lib_protocol_ltk[16];

void ns_protocol_generate_inputreport(uint8_t *report_id, uint8_t *out);

void ns_lib_protocol_generate_reply(const uint8_t *in, uint8_t *report_id, uint8_t *out);
uint8_t ns_lib_protocol_enqueue_host_input(const uint8_t *in, uint16_t in_len);

/**
 * @brief Queue one incoming host packet (OUT report) for FIFO processing.
 *
 * Processing is deferred until @ref ns_lib_protocol_generate_inputreport runs from the main loop.
 * This wrapper preserves compatibility and now aliases @ref ns_lib_protocol_enqueue_host_input.
 *
 * @return 1 when packet enqueued, else 0 (invalid args or queue full).
 */
uint8_t ns_lib_protocol_process_host_input(const uint8_t *in, uint16_t in_len, uint8_t *out_report_id,
                                           uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_PROTOCOL_H */
