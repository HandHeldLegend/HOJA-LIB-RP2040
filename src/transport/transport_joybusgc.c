#include "transport/transport_joybusgc.h"

__attribute__((weak)) bool transport_jbgc_init(core_params_s *params)
{
    return false;
}

__attribute__((weak)) void transport_jbgc_task(uint64_t timestamp)
{
    (void) timestamp;
}