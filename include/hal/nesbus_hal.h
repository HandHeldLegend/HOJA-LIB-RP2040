#ifndef HOJA_NESBUS_HAL_H
#define HOJA_NESBUS_HAL_H

#include "hoja_bsp.h"
#include "board_config.h"

#include <stdint.h>
#include <stdbool.h>

#if defined(HOJA_NESBUS_DRIVER) && (HOJA_NESBUS_DRIVER == NESBUS_DRIVER_HAL)

#if (HOJA_BSP_HAS_NESBUS==0)
    #error "NESBUS hal driver unsupported on this board" 
#endif 

#define HOJA_NESBUS_INIT() nesbus_hal_init()
#define HOJA_NESBUS_STOP() nesbus_hal_stop();
#define HOJA_NESBUS_TASK(timestamp) nesbus_hal_task(timestamp)

bool nesbus_hal_init();
void nesbus_hal_stop();
void nesbus_hal_task(uint64_t timestamp);

#endif

#endif