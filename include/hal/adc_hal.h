#ifndef HOJA_ADC_HAL_H
#define HOJA_ADC_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_bsp.h"
#include "board_config.h"

#if defined(HOJA_ADC_LX_DRIVER) && (HOJA_ADC_LX_DRIVER==ADC_DRIVER_HAL)
    #ifndef HOJA_ADC_LX_CHANNEL
        #error "Define HOJA_ADC_LX_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_LX_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_LX_PIN
        #error "Define HOJA_ADC_LX_PIN in board_config.h" 
    #endif

    #if !defined(HOJA_ADC_LX_INVERT)
        #define HOJA_ADC_LX_INVERT 0
    #endif

    #define HOJA_ADC_CHAN_LX_READ() adc_hal_read(HOJA_ADC_LX_CHANNEL, HOJA_ADC_LX_INVERT)
    #define HOJA_ADC_CHAN_LX_INIT() adc_hal_init(HOJA_ADC_LX_CHANNEL, HOJA_ADC_LX_PIN)
#endif

#if defined(HOJA_ADC_LY_DRIVER) && (HOJA_ADC_LY_DRIVER==ADC_DRIVER_HAL)
    #ifndef HOJA_ADC_LY_CHANNEL
        #error "Define HOJA_ADC_LY_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_LY_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_LY_PIN
        #error "Define HOJA_ADC_LY_PIN in board_config.h" 
    #endif

    #if !defined(HOJA_ADC_LY_INVERT)
        #define HOJA_ADC_LY_INVERT 0
    #endif

    #define HOJA_ADC_CHAN_LY_READ() adc_hal_read(HOJA_ADC_LY_CHANNEL, HOJA_ADC_LY_INVERT)
    #define HOJA_ADC_CHAN_LY_INIT() adc_hal_init(HOJA_ADC_LY_CHANNEL, HOJA_ADC_LY_PIN)
#endif

#if defined(HOJA_ADC_RX_DRIVER) && (HOJA_ADC_RX_DRIVER==ADC_DRIVER_HAL)
    #ifndef HOJA_ADC_RX_CHANNEL
        #error "Define HOJA_ADC_RX_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_RX_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_RX_PIN
        #error "Define HOJA_ADC_RX_PIN in board_config.h" 
    #endif

    #if !defined(HOJA_ADC_RX_INVERT)
        #define HOJA_ADC_RX_INVERT 0
    #endif

    #define HOJA_ADC_CHAN_RX_READ() adc_hal_read(HOJA_ADC_RX_CHANNEL, HOJA_ADC_RX_INVERT)
    #define HOJA_ADC_CHAN_RX_INIT() adc_hal_init(HOJA_ADC_RX_CHANNEL, HOJA_ADC_RX_PIN)
#endif

#if defined(HOJA_ADC_RY_DRIVER) && (HOJA_ADC_RY_DRIVER==ADC_DRIVER_HAL)
    #ifndef HOJA_ADC_RY_CHANNEL
        #error "Define HOJA_ADC_RY_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_RY_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_RY_PIN
        #error "Define HOJA_ADC_RY_PIN in board_config.h" 
    #endif

    #if !defined(HOJA_ADC_RY_INVERT)
        #define HOJA_ADC_RY_INVERT 0
    #endif

    #define HOJA_ADC_CHAN_RY_READ() adc_hal_read(HOJA_ADC_RY_CHANNEL, HOJA_ADC_RY_INVERT)
    #define HOJA_ADC_CHAN_RY_INIT() adc_hal_init(HOJA_ADC_RY_CHANNEL, HOJA_ADC_RY_PIN)
#endif

#if defined(HOJA_ADC_LT_DRIVER) && (HOJA_ADC_LT_DRIVER==ADC_DRIVER_HAL)
    #ifndef HOJA_ADC_LT_CHANNEL
        #error "Define HOJA_ADC_LT_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_LT_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_LT_PIN
        #error "Define HOJA_ADC_LT_PIN in board_config.h" 
    #endif

    #ifndef HOJA_ADC_LT_PIN
        #error "Define HOJA_ADC_LT_PIN in board_config.h" 
    #endif

    #if !defined(HOJA_ADC_LT_INVERT)
        #define HOJA_ADC_LT_INVERT 0
    #endif

    #define HOJA_ADC_CHAN_LT_READ() adc_hal_read(HOJA_ADC_LT_CHANNEL, HOJA_ADC_LT_INVERT)
    #define HOJA_ADC_CHAN_LT_INIT() adc_hal_init(HOJA_ADC_LT_CHANNEL, HOJA_ADC_LT_PIN)
#endif

#if defined(HOJA_ADC_RT_DRIVER) && (HOJA_ADC_RT_DRIVER==ADC_DRIVER_HAL)
    #ifndef HOJA_ADC_RT_CHANNEL
        #error "Define HOJA_ADC_RT_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_RT_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_RT_PIN
        #error "Define HOJA_ADC_RT_PIN in board_config.h" 
    #endif

    #if !defined(HOJA_ADC_RT_INVERT)
        #define HOJA_ADC_RT_INVERT 0
    #endif

    #define HOJA_ADC_CHAN_RT_READ() adc_hal_read(HOJA_ADC_RT_CHANNEL, HOJA_ADC_RT_INVERT)
    #define HOJA_ADC_CHAN_RT_INIT() adc_hal_init(HOJA_ADC_RT_CHANNEL, HOJA_ADC_RT_PIN)
#endif

#if defined(HOJA_ADC_BATTERY_DRIVER) && (HOJA_ADC_BATTERY_DRIVER==ADC_DRIVER_HAL)
    #ifndef HOJA_ADC_BATTERY_CHANNEL
        #error "Define HOJA_ADC_BATTERY_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_BATTERY_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_BATTERY_PIN
        #error "Define HOJA_ADC_BATTERY_PIN in board_config.h" 
    #endif

    #if !defined(HOJA_ADC_BATTERY_INVERT)
        #define HOJA_ADC_BATTERY_INVERT 0
    #endif

    #define HOJA_ADC_CHAN_BATTERY_READ() adc_hal_read(HOJA_ADC_BATTERY_CHANNEL, HOJA_ADC_BATTERY_INVERT)
    #define HOJA_ADC_CHAN_BATTERY_INIT() adc_hal_init(HOJA_ADC_BATTERY_CHANNEL, HOJA_ADC_BATTERY_PIN)
#else 
    #define BAT_EN 0
#endif
    
#define HOJA_RP2040_ADC_COUNT (LX_EN + LY_EN + RX_EN + RY_EN + LT_EN + RT_EN + BAT_EN)

uint16_t    adc_hal_read(uint8_t channel, bool invert);
bool        adc_hal_init(uint8_t channel, uint32_t gpio);

#endif