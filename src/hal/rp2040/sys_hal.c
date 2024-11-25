#include "hal/sys_hal.h"
#include "pico/stdlib.h"

void sys_hal_sleep_ms(uint32_t ms)
{
    sleep_ms(ms);
}

void sys_hal_sleep_us(uint32_t us)
{
    sleep_us(us);
}