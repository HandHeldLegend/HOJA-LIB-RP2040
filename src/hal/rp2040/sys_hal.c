#include "hal/sys_hal.h"
#include "pico/stdlib.h"

#include "hal/mutex_hal.h"
#include "pico/time.h"
#include "hardware/watchdog.h"

#define SYS_CLK_HZ 200000000

void sys_hal_reboot()
{
    // Configure the watchdog to reset the chip after a short delay
    watchdog_reboot(0, 0, 0);
    // Loop forever, waiting for the watchdog to reset the chip
    for(;;){}
}

void sys_hal_sleep_ms(uint32_t ms)
{
    sleep_ms(ms);
}

void sys_hal_sleep_us(uint32_t us)
{
    sleep_us(us);
}

void sys_hal_tick()
{
    watchdog_update();
}

bool sys_hal_init()
{
    watchdog_enable(16000, false);
    // Test overclock
    set_sys_clock_khz(SYS_CLK_HZ / 1000, true);
    return true;
}

void sys_hal_start(void (*task_core_0)(void), void (*task_core_1)(void))
{
    // Enable lockout victimhood :,)
    multicore_lockout_victim_init();

    if(task_core_1!=NULL)
        multicore_launch_core1(task_core_1);
    
    for(;;)
    {
        task_core_0();
    }
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