#ifndef HOJA_JOYBUS_GC_HAL_H
#define HOJA_JOYBUS_GC_HAL_H

#include <stdint.h>
#include <stdbool.h>

#include "hoja_bsp.h"
#include "board_config.h"

#include <stdint.h>

#if defined(HOJA_JOYBUS_GC_DRIVER) && (HOJA_JOYBUS_GC_DRIVER == JOYBUS_GC_DRIVER_HAL)

#if (HOJA_BSP_HAS_JOYBUS_GC==0)
    #error "JOYBUS hal driver unsupported on this board" 
#endif 

#define HOJA_JOYBUS_GC_INIT() joybus_gc_hal_init()
#define HOJA_JOYBUS_GC_STOP() joybus_gc_hal_stop();
#define HOJA_JOYBUS_GC_TASK(timestamp) joybus_gc_hal_task(timestamp)

void joybus_gc_hal_stop();
bool joybus_gc_hal_init();
void joybus_gc_hal_task(uint64_t timestamp);

#endif
#endif