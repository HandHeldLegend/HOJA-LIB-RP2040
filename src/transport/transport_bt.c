#include <stddef.h>

#include "transport/transport_bt.h"

__attribute__((weak)) void transport_bt_stop()
{
    
}

__attribute__((weak)) bool transport_bt_init(core_params_s *params)
{
    (void)params;
    return false;
}

__attribute__((weak)) void transport_bt_task(uint64_t timestamp)
{
    (void) timestamp;
}

__attribute__((weak)) void transport_bt_static_get_caps(transport_bt_static_caps_s *caps)
{
    if (caps == NULL)
    {
        return;
    }

    caps->bdr_supported = 0;
    caps->ble_supported = 0;
    caps->external_update_supported = 0;
}

__attribute__((weak)) uint8_t transport_bt_static_part_status(void)
{
    return TRANSPORT_WIRELESS_PART_NA;
}

__attribute__((weak)) uint16_t transport_bt_static_external_version(void)
{
    return 0;
}
