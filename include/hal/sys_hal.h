#ifndef HOJA_SYS_HAL_H
#define HOJA_SYS_HAL_H

#include <stdint.h>
#include <stdbool.h>

// Return a random uint32_t type number
uint32_t sys_hal_random();

// Update watchdog if applicable
void sys_hal_watchdog_update();

// Return time/ticks since boot 
uint32_t sys_hal_time_us();

void sys_hal_gpio_init(uint32_t gpio, bool pullup, bool input);

void sys_hal_gpio_set(uint32_t gpio, bool level);

bool sys_hal_gpio_get(uint32_t gpio);

void sys_hal_sleep_ms(uint32_t ms);

#endif