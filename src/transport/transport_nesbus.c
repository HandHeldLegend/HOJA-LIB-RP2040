#include "transport/transport_nesbus.h"

__attribute__((weak)) bool transport_nesbus_init(core_params_s *params)
{
    return false;
}

__attribute__((weak)) void transport_nesbus_task(uint64_t timestamp)
{
    (void) timestamp;
}