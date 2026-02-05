#ifndef HOJA_USB_HAL_H
#define HOJA_USB_HAL_H

#include <stdint.h>
#include "cores/cores.h"

// TODO TinyUSB specific allowances and define guards

bool usb_hal_init(core_params_s *params);

// Read SOF counter (1ms interval)
uint32_t usb_hal_sof();

#endif