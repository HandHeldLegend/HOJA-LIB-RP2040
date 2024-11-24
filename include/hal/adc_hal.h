#ifndef HOJA_ADC_HAL_H
#define HOJA_ADC_HAL_H

#include <stdint.h>
#include "hoja_bsp.h"


#ifdef HOJA_ADC_LX_DRIVER
#if HOJA_ADC_LX_DRIVER==ADC_DRIVER_HAL
    #ifndef HOJA_ADC_LX_CHANNEL
        #error "Define HOJA_ADC_LX_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_LX_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_LX_PIN
        #error "Define HOJA_ADC_LX_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_LX_READ() adc_hal_read(HOJA_ADC_LX_CHANNEL)
    #define HOJA_ADC_CHAN_LX_INIT() adc_hal_init(HOJA_ADC_LX_CHANNEL, HOJA_ADC_LX_PIN)
#endif
#endif

#ifdef HOJA_ADC_LY_DRIVER
#if HOJA_ADC_LY_DRIVER==ADC_DRIVER_HAL
    #ifndef HOJA_ADC_LY_CHANNEL
        #error "Define HOJA_ADC_LY_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_LY_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_LY_PIN
        #error "Define HOJA_ADC_LY_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_LY_READ() adc_hal_read(HOJA_ADC_LY_CHANNEL)
    #define HOJA_ADC_CHAN_LY_INIT() adc_hal_init(HOJA_ADC_LY_CHANNEL, HOJA_ADC_LY_PIN)
#endif
#endif

#ifdef HOJA_ADC_RX_DRIVER
#if HOJA_ADC_RX_DRIVER==ADC_DRIVER_HAL
    #ifndef HOJA_ADC_RX_CHANNEL
        #error "Define HOJA_ADC_RX_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_RX_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_RX_PIN
        #error "Define HOJA_ADC_RX_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_RX_READ() adc_hal_read(HOJA_ADC_RX_CHANNEL)
    #define HOJA_ADC_CHAN_RX_INIT() adc_hal_init(HOJA_ADC_RX_CHANNEL, HOJA_ADC_RX_PIN)
#endif
#endif

#ifdef HOJA_ADC_RY_DRIVER
#if HOJA_ADC_RY_DRIVER==ADC_DRIVER_HAL
    #ifndef HOJA_ADC_RY_CHANNEL
        #error "Define HOJA_ADC_RY_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_RY_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_RY_PIN
        #error "Define HOJA_ADC_RY_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_RY_READ() adc_hal_read(HOJA_ADC_RY_CHANNEL)
    #define HOJA_ADC_CHAN_RY_INIT() adc_hal_init(HOJA_ADC_RY_CHANNEL, HOJA_ADC_RY_PIN)
#endif
#endif

#ifdef HOJA_ADC_LT_DRIVER
#if HOJA_ADC_LT_DRIVER==ADC_DRIVER_HAL
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

    #define HOJA_ADC_CHAN_LT_READ() adc_hal_read(HOJA_ADC_LT_CHANNEL)
    #define HOJA_ADC_CHAN_LT_INIT() adc_hal_init(HOJA_ADC_LT_CHANNEL, HOJA_ADC_LT_PIN)
#endif
#endif

#ifdef HOJA_ADC_RT_DRIVER
#if HOJA_ADC_RT_DRIVER==ADC_DRIVER_HAL
    #ifndef HOJA_ADC_RT_CHANNEL
        #error "Define HOJA_ADC_RT_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_RT_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_RT_PIN
        #error "Define HOJA_ADC_RT_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_RT_READ() adc_hal_read(HOJA_ADC_RT_CHANNEL)
    #define HOJA_ADC_CHAN_RT_INIT() adc_hal_init(HOJA_ADC_RT_CHANNEL, HOJA_ADC_RT_PIN)
#endif
#endif

#ifdef HOJA_ADC_BATTERY_DRIVER
#if HOJA_ADC_BATTERY_DRIVER==ADC_DRIVER_HAL
    #ifndef HOJA_ADC_BATTERY_CHANNEL
        #error "Define HOJA_ADC_BATTERY_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_BATTERY_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Over the HAL defined ADC channel limit." 
    #endif

    #ifndef HOJA_ADC_BATTERY_PIN
        #error "Define HOJA_ADC_BATTERY_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_BATTERY_READ() adc_hal_read(HOJA_ADC_BATTERY_CHANNEL)
    #define HOJA_ADC_CHAN_BATTERY_INIT() adc_hal_init(HOJA_ADC_BATTERY_CHANNEL, HOJA_ADC_BATTERY_PIN)
#endif
#endif

uint32_t adc_hal_read(uint8_t channel);

bool adc_hal_init(uint8_t channel, uint32_t gpio);

#endif