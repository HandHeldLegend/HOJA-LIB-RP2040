#ifndef USB_WEBUSB_H
#define USB_WEBUSB_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  WEBUSB_INPUT_PROCESSED = 255,
  WEBUSB_INPUT_RAW = 254,
} webusb_t;

void webusb_send_bulk(const uint8_t *data, uint16_t size);
void webusb_command_handler(uint8_t *data, uint32_t size);

#endif