#ifndef HOJA_RGB_HAL_H
#define HOJA_RGB_HAL_H

#include <stdint.h>
#include "hoja_bsp.h"
#include "hoja_system.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)

#if !defined(RGB_DRIVER_OUTPUT_PIN)
    #error "RGB_DRIVER_OUTPUT_PIN undefined in board_config.h" 
#endif

#if !defined(RGB_DRIVER_COUNT)
    #error "RGB_DRIVER_COUNT (num of leds) undefined in board_config.h" 
#endif

#if !defined(RGB_DRIVER_ORDER)
    #error "RGB_DRIVER_ORDER (order of color) undefined in board_config.h" 
#endif

#define RGB_DRIVER_INIT() rgb_hal_init()
#define RGB_DRIVER_DEINIT() rgb_hal_deinit()
#define RGB_DRIVER_UPDATE(data) rgb_hal_update(data)

void rgb_hal_init();

void rgb_hal_deinit();

// Update all RGBs
void rgb_hal_update(uint32_t *data);

#endif
#endif