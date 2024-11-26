#ifndef DRIVERS_USB_MUX_PI3USB4000A
#define DRIVERS_USB_MUX_PI3USB4000A

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#if defined(USB_MUX_DRIVER_PI3USB4000A) && (USB_MUX_DRIVER_PI3USB4000A>0)

#if !defined(USB_MUX_DRIVER_ENABLE_PIN)
    #error "HOJA_DRIVER_PI3USB4000A_ENABLE_PIN required in board_config.h"
#endif 

#if !defined(USB_MUX_DRIVER_SELECT_PIN)
    #error "HOJA_DRIVER_PI3USB4000A_SELECT_PIN required in board_config.h"
#endif 

#if HOJA_USB_MUX_DRIVER==USB_MUX_DRIVER_PI3USB4000A
    #define HOJA_USB_MUX_ENABLE(enable)     pi3usb4000a_enable(enable)
    #define HOJA_USB_MUX_SELECT(channel)    pi3usb4000a_select(channel)
    #define HOJA_USB_MUX_INIT()             pi3usb4000a_init()
#endif

bool pi3usb4000a_init();

void pi3usb4000a_enable(bool enable);

void pi3usb4000a_select(uint8_t channel);

#endif

#endif