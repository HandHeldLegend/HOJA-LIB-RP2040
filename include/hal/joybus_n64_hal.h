#ifndef HOJA_JOYBUS_N64_HAL_H
#define HOJA_JOYBUS_N64_HAL_H

#include "input/button.h"
#include "input/analog.h"

#include "hoja_bsp.h"
#include "board_config.h"

#include <stdint.h>

#if defined(HOJA_JOYBUS_N64_DRIVER) && (HOJA_JOYBUS_N64_DRIVER == JOYBUS_N64_DRIVER_HAL)

#if (HOJA_BSP_HAS_JOYBUS_N64==0)
    #error "JOYBUS hal driver unsupported on this board" 
#endif 

#define HOJA_JOYBUS_N64_TASK(timestamp) joybus_n64_hal_task(timestamp)
#define HOJA_JOYBUS_N64_INIT() joybus_n64_hal_init()

void joybus_n64_hal_stop();
bool joybus_n64_hal_init();
void joybus_n64_hal_task(uint32_t timestamp);

#endif

#endif
