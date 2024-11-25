#include "hal/sys_hal.h"
#include "pico/stdlib.h"

#include "hal/mutex_hal.h"
#include "pico/time.h"

void sys_hal_sleep_ms(uint32_t ms)
{
    sleep_ms(ms);
}

void sys_hal_sleep_us(uint32_t us)
{
    sleep_us(us);
}

MUTEX_HAL_INIT(_sys_hal_time_mutex);
uint32_t sys_hal_time_us()
{
    static uint32_t time;

    if(MUTEX_HAL_ENTER_TIMEOUT_US(&_sys_hal_time_mutex, 10))
    {
        time = time_us_32();
        MUTEX_HAL_EXIT(&_sys_hal_time_mutex);
    }

    return time;
}