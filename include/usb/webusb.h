#ifndef USB_WEBUSB_H
#define USB_WEBUSB_H

#include <stdint.h>
#include <stdbool.h>
#include "settings_shared_types.h"

typedef enum
{
  WEBUSB_ID_READ_CONFIG_BLOCK = 0x1, 
  WEBUSB_ID_WRITE_CONFIG_BLOCK, 
  WEBUSB_ID_READ_STATIC_BLOCK, 
  WEBUSB_ID_CONFIG_COMMAND, 
  WEBUSB_LEGACY_SET_BOOTLOADER = 15,
  WEBUSB_LEGACY_GET_FW_VERSION = 0xAF, 
  WEBUSB_ANALOG_DUMP = 250,
  WEBUSB_INPUT_JOYSTICKS = 254,
  WEBUSB_INPUT_RAW = 255, 
} webusb_report_id_t;

bool webusb_outputting_check();
void webusb_command_confirm_cb(cfg_block_t config_block, uint8_t cmd, bool success, uint8_t *data, uint32_t size);
void webusb_send_bulk(const uint8_t *data, uint16_t size);
void webusb_command_handler(uint8_t *data, uint32_t size);
void webusb_send_rawinput(uint64_t timestamp);

#endif