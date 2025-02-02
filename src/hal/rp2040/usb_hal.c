#include "hal/usb_hal.h"

#include "hardware/structs/usb.h"

uint32_t usb_hal_sof()
{
    return usb_hw->sof_rd;
}