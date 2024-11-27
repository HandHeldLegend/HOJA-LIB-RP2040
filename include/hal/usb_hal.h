#ifndef HOJA_USB_HAL_H
#define HOJA_USB_HAL_H

// Too integrated right now to write an abstraction layer. Whatever!
// We can use this for some items that may be platform-specific and low level.

// Read SOF counter (1ms interval)
uint32_t usb_hal_sof();

#endif