#ifndef HOJA_SDRUMBLE_HAL_H
#define HOJA_SDRUMBLE_HAL_H

#include "hoja_bsp.h"
#include "devices_shared_types.h"
#include <stdint.h>

#ifdef HOJA_CONFIG_SDRUMBLE
#if (HOJA_CONFIG_SDRUMBLE==1)

#if defined(HOJA_HAPTICS_SET_STD)
    #error "HOJA_HAPTICS_SET_STD already defined. Only use SDRUMBLE or HDRUMBLE" 
#else 
    #define HOJA_HAPTICS_SET_STD(intensity) sdrumble_hal_set_standard(intensity)
#endif

#if defined(HOJA_HAPTICS_PUSH_AMFM)
    #error "HOJA_HAPTICS_PUSH_AMFM already defined. Only use SDRUMBLE or HDRUMBLE" 
#else 
    #define HOJA_HAPTICS_PUSH_AMFM(input) sdrumble_hal_push_amfm(input)
#endif

#define HOJA_HAPTICS_INIT() sdrumble_hal_init()
#define HOJA_HAPTICS_TASK(timestamp) sdrumble_hal_task(timestamp)

bool sdrumble_hal_init();
void sdrumble_hal_task(uint64_t timestamp);
void sdrumble_hal_push_amfm(haptic_processed_s *input);
void sdrumble_hal_set_standard(uint8_t intensity);

#endif
#endif
#endif