#include "hal/gpio_hal.h"

// Pico SDK specific code
#include "hardware/gpio.h"

void gpio_hal_init(uint32_t gpio, bool pullup, bool input)
{
    gpio_init(gpio);
    if(pullup) gpio_pull_up(gpio);
    gpio_set_dir(gpio, !input);
}

void gpio_hal_write(uint32_t gpio, bool level)
{
    gpio_put(gpio, level);
}

bool gpio_hal_read(uint32_t gpio)
{
    return gpio_get(gpio);
}

void gpio_hal_set_direction(uint32_t gpio, bool input)
{
    if(input)
        gpio_set_dir(gpio, GPIO_IN);
    else
        gpio_set_dir(gpio, GPIO_OUT);
}