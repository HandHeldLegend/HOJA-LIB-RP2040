#ifndef DRIVERS_HAPTIC_DRV2605L_H
#define DRIVERS_HAPTIC_DRV2605L_H

#include "hoja_bsp.h"
#include "board_config.h"
#include <stdint.h>
#include <stdbool.h>

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
#if defined(HOJA_LRA_HAL_ENABLE_DRV2605L)

#if (HOJA_BSP_HAS_I2C == 0)
    #error "DRV2605L front-end requires I2C."
#endif

// od_clamp: 0 keeps the library default OD clamp register value.
bool drv2605l_init(uint8_t i2c_instance, uint8_t od_clamp);

#endif
#endif

#endif
