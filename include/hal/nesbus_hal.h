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

typedef struct
{
    union
    {
        struct
        {
            uint8_t padding : 4;
            uint8_t r   : 1;
            uint8_t l   : 1;
            uint8_t x   : 1;
            uint8_t a   : 1;
            uint8_t dright  : 1;
            uint8_t dleft   : 1;
            uint8_t ddown   : 1;
            uint8_t dup     : 1;
            uint8_t start   : 1;
            uint8_t select  : 1;
            uint8_t y : 1;
            uint8_t b : 1;
        };
        uint16_t value;
    };
} nesbus_input_s;

bool nesbus_hal_init();
void nesbus_hal_stop();
void nesbus_hal_task(uint64_t timestamp);

#endif

#endif