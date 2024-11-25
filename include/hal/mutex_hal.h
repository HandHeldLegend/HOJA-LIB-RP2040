#ifndef HOJA_MUTEX_HAL_H
#define HOJA_MUTEX_HAL_H

#include "hoja_bsp.h"

#if HOJA_BSP_CHIPSET==CHIPSET_RP2040
    #include "pico/multicore.h"

    #define MUTEX_HAL_INIT(name) auto_init_mutex(name)

    #define MUTEX_HAL_ENTER_TIMEOUT_US(mtx, timeout_us) mutex_enter_timeout_us(mtx, timeout_us)

    #define MUTEX_HAL_ENTER_BLOCKING(mtx) mutex_enter_blocking(mtx)

    #define MUTEX_HAL_EXIT(mtx) mutex_exit(mtx)
#endif

#endif
