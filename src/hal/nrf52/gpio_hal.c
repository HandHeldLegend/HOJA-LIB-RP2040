#include "hal/gpio_hal.h"

#include "nrf52_hal.h"

static void _gpio_hal_configure(uint32_t gpio, bool input, uint32_t pull)
{
    nrf52_gpio_port(gpio)->PIN_CNF[gpio & 31u] =
        ((input ? GPIO_PIN_CNF_DIR_Input : GPIO_PIN_CNF_DIR_Output) << GPIO_PIN_CNF_DIR_Pos) |
        ((input ? GPIO_PIN_CNF_INPUT_Connect : GPIO_PIN_CNF_INPUT_Disconnect) << GPIO_PIN_CNF_INPUT_Pos) |
        (pull << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

void gpio_hal_init(uint32_t gpio, bool pullup, bool input)
{
    _gpio_hal_configure(gpio, input, pullup ? GPIO_PIN_CNF_PULL_Pullup : GPIO_PIN_CNF_PULL_Disabled);
}

void nrf52_gpio_init_input(uint32_t gpio, bool pull_up, bool pull_down)
{
    uint32_t pull = GPIO_PIN_CNF_PULL_Disabled;
    if (pull_up)
        pull = GPIO_PIN_CNF_PULL_Pullup;
    else if (pull_down)
        pull = GPIO_PIN_CNF_PULL_Pulldown;
    _gpio_hal_configure(gpio, true, pull);
}

void gpio_hal_write(uint32_t gpio, bool level)
{
    if (level)
        nrf52_gpio_port(gpio)->OUTSET = nrf52_gpio_bit(gpio);
    else
        nrf52_gpio_port(gpio)->OUTCLR = nrf52_gpio_bit(gpio);
}

bool gpio_hal_read(uint32_t gpio)
{
    return (nrf52_gpio_port(gpio)->IN & nrf52_gpio_bit(gpio)) != 0;
}

void gpio_hal_set_direction(uint32_t gpio, bool input)
{
    // Keeps the pull the pin was set up with
    uint32_t cnf = nrf52_gpio_port(gpio)->PIN_CNF[gpio & 31u];
    cnf &= ~(GPIO_PIN_CNF_DIR_Msk | GPIO_PIN_CNF_INPUT_Msk);
    cnf |= ((input ? GPIO_PIN_CNF_DIR_Input : GPIO_PIN_CNF_DIR_Output) << GPIO_PIN_CNF_DIR_Pos) |
           ((input ? GPIO_PIN_CNF_INPUT_Connect : GPIO_PIN_CNF_INPUT_Disconnect) << GPIO_PIN_CNF_INPUT_Pos);
    nrf52_gpio_port(gpio)->PIN_CNF[gpio & 31u] = cnf;
}
