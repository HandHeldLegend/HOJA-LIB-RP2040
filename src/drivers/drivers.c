#include "drivers/drivers.h"

#include "hoja_bsp.h"
#include "board_config.h"

void drivers_setup()
{
    #if defined(HAPTIC_DRIVER_DRV2605L_INIT)
        HAPTIC_DRIVER_DRV2605L_INIT();
    #endif

    #if defined(USB_MUX_DRIVER_PI3USB4000A)
        HOJA_USB_MUX_INIT();
    #endif
}