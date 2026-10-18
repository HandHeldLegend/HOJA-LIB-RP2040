#ifndef HOJA_USB_HAL_H
#define HOJA_USB_HAL_H

#include <stdint.h>
#include <stdbool.h>

// Read SOF counter (1ms interval)
uint32_t usb_hal_sof();

// Around a step that holds up the task loop for longer than TinyUSB's event queue covers (a radio
// probe): no per-frame events meanwhile, so the transfer events that keep WebUSB going still fit.
void usb_hal_frame_events_hold(bool hold);

#endif