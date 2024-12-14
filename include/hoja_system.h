#ifndef HOJA_SYSTEM_H
#define HOJA_SYSTEM_H

#include "board_config.h"

// Disable IMU function if there's no reading function
#ifdef HOJA_IMU_CHAN_A_READ
    #define HOJA_IMU_DRIVER_ENABLED 1
#else
    #define HOJA_IMU_DRIVER_ENABLED 0
#endif

// Analog smoothing
#ifdef ADC_SMOOTHING_STRENGTH
  #if (ADC_SMOOTHING_STRENGTH > 1)
    #define ADC_SMOOTHING_ENABLED 1
  #else
    #define ADC_SMOOTHING_ENABLED 0
  #endif
#else
  #warning "ADC_SMOOTHING_STRENGTH isn't defined. It's optional to smooth out analog input. Typically not recommended." 
  #define ADC_SMOOTHING_ENABLED 0
#endif

// RGB 
#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)
  #define RGB_DEVICE_ENABLED 1
#else 
  #define RGB_DEVICE_ENABLED 0
#endif

#endif