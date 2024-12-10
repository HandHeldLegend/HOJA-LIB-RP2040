#ifndef USB_WEBUSB_H
#define USB_WEBUSB_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  WEBUSB_INPUT_PROCESSED = 255,
  WEBUSB_INPUT_RAW = 254,
} webusb_t;

void webusb_save_confirm();
void webusb_command_processor(uint8_t *data);
void webusb_input_task(uint32_t timestamp);
bool webusb_output_enabled();

bool webusb_ready();
bool webusb_ready_blocking();
void webusb_enable_output(bool enable);

void webusb_send_debug_dump(uint8_t len, uint8_t *data);

#endif