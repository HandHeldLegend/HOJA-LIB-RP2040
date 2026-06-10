#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

#include "transport/transport_wlan.h"

__attribute__((weak)) void transport_wlan_stop()
{
}

__attribute__((weak)) bool transport_wlan_init(core_params_s *params)
{
    (void)params;
    return false;
}

__attribute__((weak)) void transport_wlan_task(uint64_t timestamp)
{
    (void) timestamp;
}

__attribute__((weak)) uint8_t transport_wlan_static_supported(void)
{
    return 0;
}

__attribute__((weak)) uint8_t transport_wlan_static_part_status(void)
{
    return TRANSPORT_WIRELESS_PART_NA;
}
