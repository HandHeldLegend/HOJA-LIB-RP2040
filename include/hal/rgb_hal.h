#ifndef HOJA_RGB_HAL_H
#define HOJA_RGB_HAL_H

#include <stdint.h>
#include "hoja_bsp.h"

#if (HOJA_BSP_HAS_RGB>0)

void rgb_hal_init(uint32_t gpio);

void rgb_hal_deinit();

// Update all RGBs
void rgb_hal_update(uint32_t *data);

#endif

#endif