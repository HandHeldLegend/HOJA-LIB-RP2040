#ifndef HOJA_SYS_HAL_H
#define HOJA_SYS_HAL_H

#include <stdint.h>
#include <stdbool.h>

void sys_hal_reboot();

bool sys_hal_init();

void sys_hal_start_dualcore(void (*task_core_0)(void), void (*task_core_1)(void));

// Return a random uint32_t type number
uint32_t sys_hal_random();

// Update watchdog if applicable
void sys_hal_tick();

// Return time/ticks since boot 
uint32_t sys_hal_time_us();

void sys_hal_sleep_ms(uint32_t ms);

void sys_hal_sleep_us(uint32_t us);

#endif