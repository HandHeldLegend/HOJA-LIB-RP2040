#include "hal/sys_hal.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include <stdint.h>

#include "hal/mutex_hal.h"
#include "hal/flash_hal.h"

#include "pico/time.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "pico/bootrom.h"
#include "pico/rand.h"

#include "hoja_bsp.h"

#include "board_config.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_HAL)
#include "pico/cyw43_arch.h"
#endif

void sys_hal_reboot()
{
    // Configure the watchdog to reset the chip after a short delay
    watchdog_reboot(0, 0, 0);
    // Loop forever, waiting for the watchdog to reset the chip
    for(;;){}
}

void sys_hal_bootloader()
{
    reset_usb_boot(0, 0);
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
    watchdog_enable(5000, false);
    timer_hw->dbgpause = 0;
    // Test overclock
    set_sys_clock_khz(HOJA_BSP_CLOCK_SPEED_HZ / 1000, true);
    return true;
}

void sys_hal_start_dualcore(void (*task_core_0)(void), void (*task_core_1)(void))
{
    if(task_core_1!=NULL)
        multicore_launch_core1(task_core_1);

    if(task_core_0!=NULL)
    {
        task_core_0();
    }
}

uint32_t _time_owner = 0;
volatile uint64_t _time_global = 0;

#define SYS_HAL_TIME_DEBUG 0

MUTEX_HAL_INIT(_sys_hal_time_mutex);

void sys_hal_time_us(uint64_t *out)
{
    static uint64_t time; 

    if(MUTEX_HAL_ENTER_TRY(&_sys_hal_time_mutex, &_time_owner))
    {
        #if (SYS_HAL_TIME_DEBUG==1)
        const uint64_t start_time = (uint64_t)UINT32_MAX - (uint64_t)10000000; // 10 seconds before overflow
        time = time_us_64() + start_time;
        #else
        time = time_us_64();
        #endif
        _time_global = time;
        *out = time;
        MUTEX_HAL_EXIT(&_sys_hal_time_mutex);
    }
}

void sys_hal_time_ms(uint64_t *out)
{
    static uint64_t time_ms = 0;
    if(MUTEX_HAL_ENTER_TRY(&_sys_hal_time_mutex, &_time_owner))
    {
        time_ms = _time_global/1000;
        *out = time_ms;
        MUTEX_HAL_EXIT(&_sys_hal_time_mutex);
    }
}

uint32_t sys_hal_random()
{
    return get_rand_32();
}

#define SCRATCH_OFFSET 0xC
#define MAX_INDEX     7
#define WD_READOUT_IDX 5

uint32_t sys_hal_get_bootmemory()
{
    if (WD_READOUT_IDX > MAX_INDEX) {
        // Handle the error, maybe by returning an error code or logging a message.
        // Here we just return 0 as a simple example.
        return 0;
    }
    return *((volatile uint32_t *) (WATCHDOG_BASE + SCRATCH_OFFSET + (WD_READOUT_IDX * 4)));
}

void sys_hal_set_bootmemory(uint32_t in)
{
    if (WD_READOUT_IDX > MAX_INDEX) {
        // Handle the error here. For simplicity, we'll just return in this example.
        return;
    }
    *((volatile uint32_t *) (WATCHDOG_BASE + SCRATCH_OFFSET + (WD_READOUT_IDX * 4))) = in;
}