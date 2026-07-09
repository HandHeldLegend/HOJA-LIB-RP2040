#include "transport/transport_joybus64.h"

__attribute__((weak)) bool transport_jb64_init(core_params_s *params)
{
    return false;
}

__attribute__((weak)) void transport_jb64_task(uint64_t timestamp)
{
    (void) timestamp;
}