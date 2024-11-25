#include "drivers/mux/pi3usb4000a.h"

#include "hal/gpio_hal.h"
#include "hal/sys_hal.h"

#if defined(USB_MUX_DRIVER_PI3USB4000A) && (USB_MUX_DRIVER_PI3USB4000A>0)
#if !defined(HOJA_USB_MUX_DRIVER)
    #error "HOJA_USB_MUX_DRIVER must be defined as USB_MUX_DRIVER_PI3USB4000A"
#endif

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
    _gpio_put_od(USB_MUX_DRIVER_ENABLE_PIN, !enable);
    sys_hal_sleep_ms(5);
}

void pi3usb4000a_select(uint8_t channel)
{
    if(channel>1) return;
    
    // Disable USB Mux
    pi3usb4000a_enable(false);

    sys_hal_sleep_ms(5);

    // Change channel
    if(channel)
    {
        _gpio_put_od(USB_MUX_DRIVER_SELECT_PIN, true);
    }
    else
    {
        _gpio_put_od(USB_MUX_DRIVER_SELECT_PIN, false);
    }

    sys_hal_sleep_ms(5);

    // Reenable USB Mux
    pi3usb4000a_enable(true);
}

bool pi3usb4000a_init()
{
    // Selects the USB channel
    gpio_hal_init(USB_MUX_DRIVER_SELECT_PIN, false, false);
    _gpio_put_od(USB_MUX_DRIVER_SELECT_PIN, false);

    // Enables USB device
    gpio_hal_init(USB_MUX_DRIVER_ENABLE_PIN, false, false);
    _gpio_put_od(USB_MUX_DRIVER_ENABLE_PIN, false);
}

#endif