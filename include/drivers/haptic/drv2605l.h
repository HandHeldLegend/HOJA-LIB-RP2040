#ifndef DRIVERS_HAPTIC_DRV2605L_H
#define DRIVERS_HAPTIC_DRV2605L_H

#include "hoja_bsp.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware/clocks.h"
#include "board_config.h"

#if defined(HOJA_HAPTIC_HELPER_DRIVER) && (HOJA_HAPTIC_HELPER_DRIVER == HAPTIC_HELPER_DRIVER_DRV2605L)

#if (HOJA_BSP_HAS_I2C==0)
    #error "DRV2605L helper driver requires I2C." 
#endif

#if !defined(HAPTIC_HELPER_DRIVER_DRV2605L_I2C_INSTANCE)
    #error "Define HAPTIC_HELPER_DRIVER_DRV2605L_I2C_INSTANCE in board_config.h" 
#endif 

#define HOJA_HAPTIC_HELPER_DRIVER_INIT() drv2605l_init(HAPTIC_HELPER_DRIVER_DRV2605L_I2C_INSTANCE)

bool drv2605l_init(uint8_t i2c_instance);

#endif

#endif