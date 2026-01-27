#include "board_config.h"
#include "devices/wlan.h"
#include "devices_shared_types.h"
#include "hoja.h"

#if defined(HOJA_WLAN_DRIVER) && (HOJA_WLAN_DRIVER == WLAN_DRIVER_HAL)
    #include "hal/wlan_hal.h"
    #warning "HAL WLAN IN USE"
#endif

bool wlan_mode_start(gamepad_mode_t mode, bool pairing_mode)
{
    #if defined(HOJA_WLAN_DRIVER) && (HOJA_WLAN_DRIVER>0)

        #if defined(HOJA_WLAN_INIT)
        return HOJA_WLAN_INIT(mode, pairing_mode);
        #else
        return false;
        #endif

    #endif
}

bool wlan_mode_task(uint64_t timestamp)
{
    #if defined(HOJA_WLAN_TASK)
    return HOJA_WLAN_TASK(timestamp);
    #endif

    return true;
}
