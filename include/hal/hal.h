#ifndef HOJA_HAL_H
#define HOJA_HAL_H

#include "hoja_bsp.h"

#ifdef HOJA_BSP_HAS_SPI
#if HOJA_BSP_HAS_SPI > 0
#include "hal/spi_hal.h"
#endif
#endif

#ifdef HOJA_BSP_HAS_ADC
#if HOJA_BSP_HAS_ADC > 0
#include "hal/adc_hal.h"
#endif
#endif

#ifdef HOJA_BSP_HAS_I2C
#if HOJA_BSP_HAS_I2C > 0
#include "hal/i2c_hal.h"
#endif
#endif

#ifdef HOJA_BSP_HAS_USB
#if HOJA_BSP_HAS_USB > 0
#include "hal/usb_hal.h"
#endif
#endif

#ifdef HOJA_BSP_HAS_RGB
#if HOJA_BSP_HAS_RGB > 0
#include "hal/rgb_hal.h"
#endif
#endif

#ifdef HOJA_BSP_HAS_GCJOYBUS
#if HOJA_BSP_HAS_GCJOYBUS > 0
#include "hal/joybus_gc_hal.h"
#endif
#endif

#ifdef HOJA_BSP_HAS_N64JOYBUS
#if HOJA_BSP_HAS_N64JOYBUS > 0
#include "hal/joybus_n64_hal.h"
#endif
#endif

#ifdef HOJA_BSP_HAS_NESBUS
#if HOJA_BSP_HAS_NESBUS > 0
#include "hal/nesbus_hal.h"
#endif
#endif

#if (HOJA_BSP_HAS_HDRUMBLE==1)
#include "hal/hdrumble_hal.h"
#endif

bool hal_init();

#endif