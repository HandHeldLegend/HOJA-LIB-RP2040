#ifndef HOJA_RGB_HAL_H
#define HOJA_RGB_HAL_H

#include <stdbool.h>
#include <stdint.h>
#include "hoja_bsp.h"
#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)

#if (HOJA_BSP_CHIPSET == CHIPSET_NRF52)
// RGB driver config on the nRF52: one status LED, a PWM pin per colour (set RGB_DRIVER_LED_COUNT to
// 1). pin_w is a white LED beside it that is held off, or NRF52_PIN_NONE.
typedef struct
{
    uint8_t pin_r;
    uint8_t pin_g;
    uint8_t pin_b;
    uint8_t pin_w;
    bool    active_low; // The LEDs light with their pin low
} rgb_hal_cfg_s;
#else
// RGB driver line config. The WS2812 data GPIO is supplied at runtime; the PIO
// block + state machine are allocated dynamically at init. LED count
// (RGB_DRIVER_LED_COUNT) and color order (RGB_DRIVER_ORDER) remain compile-time
// (they size buffers / shape rgb_s) and default in devices_shared_types.h.
typedef struct
{
    uint8_t gpio_pin; // WS2812 data output GPIO
} rgb_hal_cfg_s;
#endif

#define RGB_DRIVER_INIT() rgb_hal_init()
#define RGB_DRIVER_DEINIT() rgb_hal_deinit()
#define RGB_DRIVER_UPDATE(data) rgb_hal_update(data)
#define RGB_DRIVER_FRAMES() rgb_hal_frames()

void rgb_hal_init();

void rgb_hal_deinit();

// Update all RGBs
void rgb_hal_update(rgb_s *data);

#if (RGB_DRIVER_OUTPUT_FRAMES > 1)
// The RGB_DITHER_FRAMES frames (RGB_DRIVER_LED_COUNT each, back to back) the hardware plays in turn
rgb_s *rgb_hal_frames(void);
#endif

#endif
#endif