#include "drivers/mux/tmux1204.h"

#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"

#include <stdlib.h>

bool tmux1204_init(mux_tmux1204_driver_s *driver)
{
    gpio_hal_init(driver->a0_gpio, true, false);
    gpio_hal_init(driver->a1_gpio, true, false);
    driver->initialized = true;
    return true;
}

bool tmux1204_switch_channel(mux_tmux1204_driver_s *driver, uint8_t ch)
{
    if(!driver->initialized) return false;

    uint8_t a0_gpio = driver->a0_gpio;
    uint8_t a1_gpio = driver->a1_gpio;

    // Set MUX position based on channel config
    switch(ch)
    {
        // S1
        case 0:
            gpio_hal_write(a0_gpio, false);
            gpio_hal_write(a1_gpio, false);
        break;

        // S2
        case 1:
            gpio_hal_write(a0_gpio, true);
            gpio_hal_write(a1_gpio, false);
        break;

        // S3
        case 2:
            gpio_hal_write(a0_gpio, false);
            gpio_hal_write(a1_gpio, true);
        break;

        // S4
        case 3:
            gpio_hal_write(a0_gpio, true);
            gpio_hal_write(a1_gpio, true);
        break;
    }
    sys_hal_sleep_us(16);
    return true;
}
