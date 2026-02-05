#ifndef SWITCH_COMMANDS_H
#define SWITCH_COMMANDS_H

#include <stdint.h>
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
void swcmd_generate_reply(uint8_t *in, uint8_t *report_id, uint8_t *out);

#endif
