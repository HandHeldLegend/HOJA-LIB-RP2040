/**
 * @file ns_lib_protocol.c
 * @brief OUT-report command parsing and input/command report helpers (NS-LIB-HID; migrated from HOJA switch_commands).
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

#include "ns_lib.h"
#include "ns_lib_protocol.h"
#include "ns_lib_config.h"
#include "ns_lib_spi.h"
#include "ns_lib_types.h"
#include "ns_lib_haptics.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Common report/packet offsets */
#define NS_LIB_PROTOCOL_IDX_TIMER 0u
#define NS_LIB_PROTOCOL_IDX_BATTCONN 1u
#define NS_LIB_PROTOCOL_IDX_SUBCMD 10u
#define NS_LIB_PROTOCOL_IDX_SUBCMD_ARG0 11u
#define NS_LIB_PROTOCOL_IDX_SUBCMD_ARG1 12u
#define NS_LIB_PROTOCOL_IDX_ACK 12u
#define NS_LIB_PROTOCOL_IDX_ACK_SUBCMD 13u
#define NS_LIB_PROTOCOL_IDX_PAYLOAD 14u
#define NS_LIB_PROTOCOL_IDX_PAYLOAD2 15u
#define NS_LIB_PROTOCOL_IDX_IMU_DATA_START 12u
#define NS_LIB_PROTOCOL_IDX_INFO_CODE 1u

#define NS_LIB_PROTOCOL_IDX_INFO_PAYLOAD 0u
#define NS_LIB_PROTOCOL_IDX_INFO_MAC0 3u
#define NS_LIB_PROTOCOL_IDX_INFO_MAC1 4u
#define NS_LIB_PROTOCOL_IDX_INFO_MAC2 5u
#define NS_LIB_PROTOCOL_IDX_INFO_MAC3 6u
#define NS_LIB_PROTOCOL_IDX_INFO_MAC4 7u
#define NS_LIB_PROTOCOL_IDX_INFO_MAC5 8u

#define NS_LIB_PROTOCOL_REPORT_ID_30 0x30u
#define NS_LIB_PROTOCOL_REPLY_ID_21 0x21u
#define NS_LIB_PROTOCOL_REPLY_ID_81 0x81u
#define NS_LIB_PROTOCOL_MAX_PACKET_BYTES 64u
#define NS_LIB_PROTOCOL_CMD_FIFO_DEPTH 3u

uint8_t ns_lib_protocol_in_command_buffer[64] = {0};
uint8_t ns_lib_protocol_in_report_id = 0x00;
uint16_t ns_lib_protocol_in_command_len = 64;
uint8_t ns_lib_protocol_in_command_got = 0;

uint8_t ns_lib_protocol_reporting_mode = NS_LIB_PROTOCOL_REPORT_ID_30;
uint8_t ns_lib_protocol_imu_mode = 0x00;

uint8_t ns_lib_protocol_command_buffer[64] = {0};
uint8_t ns_lib_protocol_command_report_id = 0x00;

uint8_t ns_lib_protocol_ltk[16] = {0};
typedef struct
{
    uint16_t len;
    uint8_t data[NS_LIB_PROTOCOL_MAX_PACKET_BYTES];
} ns_lib_protocol_pending_packet_s;

static ns_lib_protocol_pending_packet_s s_ns_lib_protocol_queue[NS_LIB_PROTOCOL_CMD_FIFO_DEPTH];
static uint8_t s_ns_lib_protocol_queue_head = 0u;
static uint8_t s_ns_lib_protocol_queue_tail = 0u;
static uint8_t s_ns_lib_protocol_queue_count = 0u;

static int _ns_lib_protocol_mac_is_nonzero(const uint8_t mac[6])
{
    for (int i = 0; i < 6; i++)
    {
        if (mac[i] != 0u)
        {
            return 1;
        }
    }
    return 0;
}

static void _ns_lib_protocol_get_device_mac(uint8_t mac[6])
{
    ns_device_config_s cfg = {0};
    ns_device_config_get(&cfg);
    memcpy(mac, cfg.device_mac, 6);
}

static void _ns_protocol_command_handler(uint8_t command, const uint8_t *data, uint8_t *out);
static void _ns_protocol_generate_standard_inputreport(uint8_t *report_id, uint8_t *out);
static uint8_t _ns_protocol_outputqueue_pop(ns_lib_protocol_pending_packet_s *out_packet);

static void _generate_ltk(void)
{
    for (uint8_t i = 0; i < 16; i++)
    {
        ns_lib_protocol_ltk[i] = ns_get_random_u8();
    }
}

static void _ns_protocol_set_ack(uint8_t ack, uint8_t *target)
{
    target[NS_LIB_PROTOCOL_IDX_ACK] = ack;
}

static void _ns_lib_protocol_set_command(uint8_t command, uint8_t *target)
{
    target[NS_LIB_PROTOCOL_IDX_ACK_SUBCMD] = command;
}

static void _ns_lib_protocol_set_timer(uint8_t *target)
{
    static uint64_t this_ms = 0;
    ns_get_time_ms(&this_ms);

    static uint64_t ns_lib_protocol_timer = 0;
    ns_lib_protocol_timer += this_ms;
    if (ns_lib_protocol_timer > 0xFF)
    {
        ns_lib_protocol_timer %= 0xFF;
    }
    target[NS_LIB_PROTOCOL_IDX_TIMER] = (uint8_t)ns_lib_protocol_timer;
}

static void _ns_lib_protocol_set_battconn(uint8_t *target)
{
    ns_powerstatus_s ps = {0};
    ns_get_powerstatus_cb(&ps);
    target[NS_LIB_PROTOCOL_IDX_BATTCONN] = ps.val;
}

static void _ns_protocol_set_devinfo(uint8_t *target)
{
    target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 0] = 0x04;
    target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 1] = 0x33;
    target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 2] = 0x03;
    target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 3] = 0x02;

    uint8_t mac[6];
    _ns_lib_protocol_get_device_mac(mac);
    memcpy(&target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 4], mac, 6);

    target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 10] = 0x00;
    target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 11] = 0x02;
}

static void _ns_protocol_set_subtriggertime(uint16_t time_10_ms, uint8_t *target)
{
    uint8_t upper_ms = 0xFF & time_10_ms;
    uint8_t lower_ms = (uint8_t)((0xFF00 & time_10_ms) >> 8);

    for (uint8_t i = 0; i < 14; i += 2)
    {
        target[NS_LIB_PROTOCOL_IDX_PAYLOAD + i] = upper_ms;
        target[NS_LIB_PROTOCOL_IDX_PAYLOAD2 + i] = lower_ms;
    }
}

static void _ns_lib_protocol_info_set_mac(uint8_t *target)
{
    target[NS_LIB_PROTOCOL_IDX_INFO_PAYLOAD + 0] = 0x01;
    target[NS_LIB_PROTOCOL_IDX_INFO_PAYLOAD + 1] = 0x00;
    target[NS_LIB_PROTOCOL_IDX_INFO_PAYLOAD + 2] = 0x03;

    uint8_t mac[6];
    _ns_lib_protocol_get_device_mac(mac);

    target[NS_LIB_PROTOCOL_IDX_INFO_MAC0] = mac[5];
    target[NS_LIB_PROTOCOL_IDX_INFO_MAC1] = mac[4];
    target[NS_LIB_PROTOCOL_IDX_INFO_MAC2] = mac[3];
    target[NS_LIB_PROTOCOL_IDX_INFO_MAC3] = mac[2];
    target[NS_LIB_PROTOCOL_IDX_INFO_MAC4] = mac[1];
    target[NS_LIB_PROTOCOL_IDX_INFO_MAC5] = mac[0];
}

static void _ns_protocol_info_handler(uint8_t info_code, uint8_t *target)
{
    switch (info_code)
    {
    case 0x01:
        _ns_lib_protocol_info_set_mac(target);
        break;
    default:
        target[NS_LIB_PROTOCOL_IDX_INFO_PAYLOAD] = info_code;
        break;
    }
}

static void _ns_protocol_pairing_set(uint8_t phase, const uint8_t *host_address, uint8_t *target)
{
    const uint8_t pro_controller_string[24] = {0x00, 0x25, 0x08, 0x50, 0x72, 0x6F, 0x20, 0x43, 0x6F,
                                                0x6E, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x65, 0x72, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68};
    (void)host_address;

    switch (phase)
    {
    default:
    case 1:
        _ns_protocol_set_ack(0x81, target);
        target[NS_LIB_PROTOCOL_IDX_PAYLOAD] = 1;
        {
            uint8_t mac[6];
            _ns_lib_protocol_get_device_mac(mac);
            target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 1] = mac[5];
            target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 2] = mac[4];
            target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 3] = mac[3];
            target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 4] = mac[2];
            target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 5] = mac[1];
            target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 6] = mac[0];
        }
        memcpy(&target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 7], pro_controller_string, 24);
        break;
    case 2:
        _ns_protocol_set_ack(0x81, target);
        target[NS_LIB_PROTOCOL_IDX_PAYLOAD] = 2;
        for (int i = 0; i < 16; i++)
        {
            target[NS_LIB_PROTOCOL_IDX_PAYLOAD + 1 + i] = ns_lib_protocol_ltk[i] ^ 0xAA;
        }
        break;
    case 3:
        _ns_protocol_set_ack(0x81, target);
        target[NS_LIB_PROTOCOL_IDX_PAYLOAD] = 3;
        break;
    }
}

void ns_protocol_generate_inputreport(uint8_t *report_id, uint8_t *out)
{
    ns_lib_protocol_pending_packet_s pending = {0};
    if (report_id == NULL || out == NULL)
    {
        return;
    }

    if (_ns_protocol_outputqueue_pop(&pending))
    {
        // Get output report ID
        uint8_t out_id = pending.data[0];

        if ((out_id == NS_LIB_PROTOCOL_OUT_ID_RUMBLE || out_id == NS_LIB_PROTOCOL_OUT_ID_RUMBLE_CMD)
            && pending.len >= 6u)
        {
            ns_haptics_rumble_translate(&pending.data[2]);
        }
        if (out_id == NS_LIB_PROTOCOL_OUT_ID_RUMBLE_CMD || out_id == NS_LIB_PROTOCOL_OUT_ID_INFO)
        {
            ns_lib_protocol_generate_reply(pending.data, report_id, out);
            return;
        }
    }

    // Send standard input report (0x30)
    _ns_protocol_generate_standard_inputreport(report_id, out);
}

static void _ns_protocol_command_handler(uint8_t command, const uint8_t *data, uint8_t *out)
{
    _ns_lib_protocol_set_timer(out);
    _ns_lib_protocol_set_battconn(out);
    _ns_lib_protocol_set_command(command, out);

    switch (command)
    {
    case NS_LIB_PROTOCOL_SET_NFC:
        _ns_protocol_set_ack(0x80, out);
        break;
    case NS_LIB_PROTOCOL_ENABLE_IMU:
        ns_lib_protocol_imu_mode = data[NS_LIB_PROTOCOL_IDX_SUBCMD_ARG0];
        ns_set_imumode_cb(ns_lib_protocol_imu_mode);
        _ns_protocol_set_ack(0x80, out);
        break;
    case NS_LIB_PROTOCOL_SET_PAIRING:
        _ns_protocol_pairing_set(data[NS_LIB_PROTOCOL_IDX_SUBCMD_ARG0], &data[NS_LIB_PROTOCOL_IDX_SUBCMD_ARG1], out);
        break;
    case NS_LIB_PROTOCOL_SET_INPUTMODE:
        _ns_protocol_set_ack(0x80, out);
        ns_lib_protocol_reporting_mode = data[NS_LIB_PROTOCOL_IDX_SUBCMD_ARG0];
        break;
    case NS_LIB_PROTOCOL_GET_DEVICEINFO:
        _ns_protocol_set_ack(0x82, out);
        _ns_protocol_set_devinfo(out);
        break;
    case NS_LIB_PROTOCOL_SET_SHIPMODE:
        _ns_protocol_set_ack(0x80, out);
        break;
    case NS_LIB_PROTOCOL_GET_SPI:
        _ns_protocol_set_ack(0x90, out);
        ns_spi_get(data[NS_LIB_PROTOCOL_IDX_SUBCMD_ARG1], data[NS_LIB_PROTOCOL_IDX_SUBCMD_ARG0],
                       data[NS_LIB_PROTOCOL_IDX_PAYLOAD2], out);
        break;
    case NS_LIB_PROTOCOL_SET_HCI:
        break;
    case NS_LIB_PROTOCOL_SET_SPI:
        _ns_protocol_set_ack(0x80, out);
        break;
    case NS_LIB_PROTOCOL_GET_TRIGGERET:
        _ns_protocol_set_ack(0x83, out);
        _ns_protocol_set_subtriggertime(100, out);
        break;
    case NS_LIB_PROTOCOL_ENABLE_VIBRATE:
        _ns_protocol_set_ack(0x80, out);
        break;
    case NS_LIB_PROTOCOL_SET_PLAYER:
    {
        _ns_protocol_set_ack(0x80, out);
        uint8_t player = data[NS_LIB_PROTOCOL_IDX_SUBCMD_ARG0] & 0xF;
        uint8_t set_num = 0;
        switch (player)
        {
        default:
            break;
        case 0b1:
            set_num = 1;
            break;
        case 0b11:
            set_num = 2;
            break;
        case 0b111:
            set_num = 3;
            break;
        case 0b1111:
            set_num = 4;
            break;
        case 0b1001:
            set_num = 5;
            break;
        case 0b1010:
            set_num = 6;
            break;
        case 0b1011:
            set_num = 7;
            break;
        case 0b0110:
            set_num = 8;
            break;
        }

        ns_set_led_cb(set_num);
    }
    break;
    default:
        _ns_protocol_set_ack(0x80, out);
        break;
    }
}

void ns_lib_protocol_generate_reply(const uint8_t *in, uint8_t *report_id, uint8_t *out)
{
    switch (in[0])
    {
    case NS_LIB_PROTOCOL_OUT_ID_RUMBLE_CMD:
        *report_id = NS_LIB_PROTOCOL_REPLY_ID_21;
        _ns_protocol_command_handler(in[NS_LIB_PROTOCOL_IDX_SUBCMD], in, out);
        break;
    case NS_LIB_PROTOCOL_OUT_ID_INFO:
        *report_id = NS_LIB_PROTOCOL_REPLY_ID_81;
        _ns_protocol_info_handler(in[NS_LIB_PROTOCOL_IDX_INFO_CODE], out);
        break;
    default:
        break;
    }
}

uint8_t ns_lib_protocol_process_host_input(const uint8_t *in, uint16_t in_len, uint8_t *out_report_id,
                                           uint8_t *out)
{
    (void)out_report_id;
    (void)out;
    return ns_lib_protocol_enqueue_host_input(in, in_len);
}

uint8_t ns_lib_protocol_enqueue_host_input(const uint8_t *in, uint16_t in_len)
{
    if (in == NULL || in_len == 0u)
    {
        return 0u;
    }
    if (in_len > NS_LIB_PROTOCOL_MAX_PACKET_BYTES)
    {
        in_len = NS_LIB_PROTOCOL_MAX_PACKET_BYTES;
    }
    if (s_ns_lib_protocol_queue_count >= NS_LIB_PROTOCOL_CMD_FIFO_DEPTH)
    {
        return 0u;
    }

    ns_lib_protocol_pending_packet_s *slot = &s_ns_lib_protocol_queue[s_ns_lib_protocol_queue_tail];
    slot->len = in_len;
    memcpy(slot->data, in, in_len);

    s_ns_lib_protocol_queue_tail = (uint8_t)((s_ns_lib_protocol_queue_tail + 1u) % NS_LIB_PROTOCOL_CMD_FIFO_DEPTH);
    s_ns_lib_protocol_queue_count++;
    return 1u;
}

static void _ns_protocol_generate_standard_inputreport(uint8_t *report_id, uint8_t *out)
{
    *report_id = NS_LIB_PROTOCOL_REPORT_ID_30;
    _ns_lib_protocol_set_timer(out);
    _ns_lib_protocol_set_battconn(out);

    ns_get_inputdata_cb((ns_inputdata_s*) out[3]);

    static ns_gyrodata_s gyro = {0};
    static ns_mode_2_s mode_2_data = {0};
    static ns_quaternion_s quat = {0};

    switch (ns_lib_protocol_imu_mode)
    {
    case NS_IMU_RAW:
        // Callback to get user gyro data
        ns_get_imu_standard_cb(&gyro);
        // Group 1
        out[12] = gyro.ay_8l; // Y-axis
        out[13] = gyro.ay_8h;
        out[14] = gyro.ax_8l; // X-axis
        out[15] = gyro.ax_8h;
        out[16] = gyro.az_8l; // Z-axis
        out[17] = gyro.az_8h;
        out[18] = gyro.gy_8l;
        out[19] = gyro.gy_8h;
        out[20] = gyro.gx_8l;
        out[21] = gyro.gx_8h;
        out[22] = gyro.gz_8l;
        out[23] = gyro.gz_8h;
        // Group 2
        memcpy(&out[24], &out[12], 12);
        // Group 3
        memcpy(&out[36], &out[12], 12);
        break;
    case NS_IMU_QUAT:
    {
        // Callback to get user quaternion data
        ns_get_imu_quaternion_cb(&quat);

        ns_motion_pack_quat(&quat, &mode_2_data);
        memcpy(&(out[NS_LIB_PROTOCOL_IDX_IMU_DATA_START]), &mode_2_data, sizeof(ns_mode_2_s));
    }
    break;

    case NS_IMU_OFF:
    default:
        break;
    }
}

static uint8_t _ns_protocol_outputqueue_pop(ns_lib_protocol_pending_packet_s *out_packet)
{
    if (out_packet == NULL || s_ns_lib_protocol_queue_count == 0u)
    {
        return 0u;
    }

    *out_packet = s_ns_lib_protocol_queue[s_ns_lib_protocol_queue_head];

    s_ns_lib_protocol_queue_head = (uint8_t)((s_ns_lib_protocol_queue_head + 1u) % NS_LIB_PROTOCOL_CMD_FIFO_DEPTH);
    s_ns_lib_protocol_queue_count--;
    return 1u;
}
