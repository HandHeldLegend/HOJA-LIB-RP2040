#ifndef HOJA_GPIO_HAL_H
#define HOJA_GPIO_HAL_H

#include <stdint.h>
#include <stdbool.h>

void gpio_hal_init(uint32_t gpio, bool pullup, bool input);

void gpio_hal_write(uint32_t gpio, bool level);

bool gpio_hal_read(uint32_t gpio);

#endif