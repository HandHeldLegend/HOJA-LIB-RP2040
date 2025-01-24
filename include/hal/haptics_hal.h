#ifndef HOJA_HAPTICS_HAL_H
#define HOJA_HAPTICS_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER==HAPTICS_DRIVER_HAL)
#if defined(HOJA_HAPTICS_SET_STD)
    #error "HOJA_HAPTICS_SET_STD already defined." 
#else 
    #define HOJA_HAPTICS_SET_STD(intensity) haptics_hal_set_standard(intensity)
#endif

#if defined(HOJA_HAPTICS_PUSH_AMFM)
    #error "HOJA_HAPTICS_PUSH_AMFM already defined." 
#else 
    #define HOJA_HAPTICS_PUSH_AMFM(input) haptics_hal_push_amfm(input)
#endif

#define HOJA_HAPTICS_INIT(intensity) haptics_hal_init(intensity)
#define HOJA_HAPTICS_TASK(timestamp) haptics_hal_task(timestamp)
#define HOJA_HAPTICS_STOP() haptics_hal_stop()

void haptics_hal_stop();
bool haptics_hal_init(uint8_t intensity);
void haptics_hal_task(uint32_t timestamp);
void haptics_hal_push_amfm(haptic_processed_s *input);
void haptics_hal_set_standard(uint8_t intensity);
#endif

#endif