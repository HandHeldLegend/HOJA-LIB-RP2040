#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

#include "transport/transport_wlan.h"

__attribute__((weak)) bool transport_wlan_init(core_params_s *params)
{
    return false;
}

__attribute__((weak)) void transport_wlan_task(uint64_t timestamp)
{
    (void) timestamp;
}