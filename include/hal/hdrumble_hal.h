#ifndef HOJA_HDRUMBLE_HAL_H
#define HOJA_HDRUMBLE_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "board_config.h"
#include "devices_shared_types.h"

#ifdef HOJA_CONFIG_HDRUMBLE
#if (HOJA_CONFIG_HDRUMBLE==1)

#if defined(HOJA_HAPTICS_SET_STD)
    #error "HOJA_HAPTICS_SET_STD already defined. Only use SDRUMBLE or HDRUMBLE" 
#else 
    #define HOJA_HAPTICS_SET_STD(intensity) hdrumble_hal_set_standard(intensity)
#endif

#if defined(HOJA_HAPTICS_PUSH_AMFM)
    #error "HOJA_HAPTICS_PUSH_AMFM already defined. Only use SDRUMBLE or HDRUMBLE" 
#else 
    #define HOJA_HAPTICS_PUSH_AMFM(input) hdrumble_hal_push_amfm(input)
#endif

#define HOJA_HAPTICS_INIT() hdrumble_hal_init()
#define HOJA_HAPTICS_TASK(timestamp) hdrumble_hal_task(timestamp)

bool hdrumble_hal_init();
void hdrumble_hal_task(uint32_t timestamp);
void hdrumble_hal_push_amfm(haptic_processed_s *input);
void hdrumble_hal_set_standard(uint8_t intensity);

#endif
#endif

#endif