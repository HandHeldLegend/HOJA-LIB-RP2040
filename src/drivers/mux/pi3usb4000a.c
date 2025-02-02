#include "drivers/mux/pi3usb4000a.h"

#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"

#if defined(HOJA_USB_MUX_DRIVER) && (HOJA_USB_MUX_DRIVER==USB_MUX_DRIVER_PI3USB4000A)

void _gpio_put_od(uint32_t gpio, bool level)
{
    if(level)
    {
        gpio_hal_set_direction(gpio, true);
        gpio_hal_write(gpio, true);
    }
    else
    {
        gpio_hal_set_direction(gpio, false);
        gpio_hal_write(gpio, false);
    }
}

void pi3usb4000a_enable(bool enable)
{
    // LOW signal enables the USB output
    gpio_hal_write(USB_MUX_DRIVER_ENABLE_PIN, !enable);
    sys_hal_sleep_ms(5);
}

void pi3usb4000a_select(uint8_t channel)
{
    if(channel>1) return;

    // Change channel
    if(channel)
    {
        gpio_hal_write(USB_MUX_DRIVER_SELECT_PIN, true);
    }
    else
    {
        gpio_hal_write(USB_MUX_DRIVER_SELECT_PIN, false);
    }
}

bool pi3usb4000a_init()
{
    // Selects the USB channel
    gpio_hal_init(USB_MUX_DRIVER_SELECT_PIN, false, false);
    gpio_hal_write(USB_MUX_DRIVER_SELECT_PIN, false);

    // Enables USB device
    gpio_hal_init(USB_MUX_DRIVER_ENABLE_PIN, false, false);
    gpio_hal_write(USB_MUX_DRIVER_ENABLE_PIN, false);

    return true;
}

#endif