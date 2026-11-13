#ifndef HOJA_MUTEX_HAL_H
#define HOJA_MUTEX_HAL_H

#include "hoja_bsp.h"

#if (HOJA_BSP_CHIPSET == CHIPSET_RP2040)
    #include "pico/multicore.h"

    #define MUTEX_HAL_INIT(name) auto_init_mutex(name)

    #define MUTEX_HAL_ENTER_TIMEOUT_US(mtx, timeout_us) mutex_enter_timeout_us(mtx, timeout_us)

    #define MUTEX_HAL_ENTER_BLOCKING(mtx) mutex_enter_blocking(mtx)

    #define MUTEX_HAL_EXIT(mtx) mutex_exit(mtx)

    #define MUTEX_HAL_ENTER_TRY(mtx, owner) mutex_try_enter(mtx, owner)
#elif (HOJA_BSP_CHIPSET == CHIPSET_NRF52)
    // One core, its two loops taking turns, and nothing takes these from an interrupt: a mutex is
    // always free when it is asked for
    #include <stdbool.h>
    #include <stdint.h>

    typedef uint8_t mutex_hal_t;

    #define MUTEX_HAL_INIT(name) static mutex_hal_t name __attribute__((unused))

    #define MUTEX_HAL_ENTER_TIMEOUT_US(mtx, timeout_us) ((void)(mtx), (void)(timeout_us), true)

    #define MUTEX_HAL_ENTER_BLOCKING(mtx) ((void)(mtx))

    #define MUTEX_HAL_EXIT(mtx) ((void)(mtx))

    #define MUTEX_HAL_ENTER_TRY(mtx, owner) ((void)(mtx), (void)(owner), true)
#endif

#endif
