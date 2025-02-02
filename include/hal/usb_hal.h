#ifndef HOJA_USB_HAL_H
#define HOJA_USB_HAL_H

#include <stdint.h>

// TODO TinyUSB specific allowances and define guards

// Read SOF counter (1ms interval)
uint32_t usb_hal_sof();

#endif