#ifndef HOJA_RGB_HAL_H
#define HOJA_RGB_HAL_H

#include <stdint.h>
#include "hoja_bsp.h"
#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)

// RGB driver line config. The WS2812 data GPIO is supplied at runtime; the PIO
// block + state machine are allocated dynamically at init. LED count
// (RGB_DRIVER_LED_COUNT) and color order (RGB_DRIVER_ORDER) remain compile-time
// (they size buffers / shape rgb_s) and default in devices_shared_types.h.
typedef struct
{
    uint8_t gpio_pin; // WS2812 data output GPIO
} rgb_hal_cfg_s;

#define RGB_DRIVER_INIT() rgb_hal_init()
#define RGB_DRIVER_DEINIT() rgb_hal_deinit()
#define RGB_DRIVER_UPDATE(data) rgb_hal_update(data)

void rgb_hal_init();

void rgb_hal_deinit();

// Update all RGBs
void rgb_hal_update(rgb_s *data);

#endif
#endif