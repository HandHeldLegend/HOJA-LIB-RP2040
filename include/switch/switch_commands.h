#ifndef SWITCH_COMMANDS_H
#define SWITCH_COMMANDS_H

#include <stdint.h>
#include "usb/swpro.h"
#include "cores/core_switch.h"
#include "hoja_shared_types.h"

#define SW_OUT_ID_RUMBLE_CMD 0x01
#define SW_OUT_ID_INFO  0x80
#define SW_OUT_ID_RUMBLE 0x10

#define SW_CMD_GET_STATE        0x00
#define SW_CMD_SET_PAIRING      0x01
#define SW_CMD_GET_DEVICEINFO   0x02
#define SW_CMD_SET_INPUTMODE    0x03
#define SW_CMD_GET_TRIGGERET    0x04
#define SW_CMD_GET_PAGELIST     0x05
#define SW_CMD_SET_HCI          0x06
#define SW_CMD_SET_SHIPMODE     0x08
#define SW_CMD_GET_SPI          0x10
#define SW_CMD_SET_SPI          0x11
#define SW_CMD_SET_NFC          0x21
#define SW_CMD_SET_NFC_STATE    0x22
#define SW_CMD_ENABLE_IMU       0x40
#define SW_CMD_SET_IMUSENS      0x41
#define SW_CMD_ENABLE_VIBRATE   0x48
#define SW_CMD_SET_PLAYER       0x30
#define SW_CMD_GET_PLAYER       0x31
#define SW_CMD_33               0x33

void swcmd_generate_inputreport(uint8_t *report_id, uint8_t *out);
void swcmd_generate_reply(ns_command_data_s *data, uint8_t *report_id, uint8_t *out);

void switch_commands_process(uint64_t timestamp, sw_input_s *input_data, hid_report_tunnel_cb cb);
void switch_commands_future_handle(uint8_t command_id, const uint8_t *data, uint16_t len);
void switch_commands_bulkset(uint8_t start_idx, uint8_t* data, uint8_t len);

#endif
