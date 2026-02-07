#include "transport/transport_bt.h"

__attribute__((weak)) void transport_bt_stop()
{
    
}

__attribute__((weak)) bool transport_bt_init(core_params_s *params)
{
    return false;
}

__attribute__((weak)) void transport_bt_task(uint64_t timestamp)
{
    (void) timestamp;
}

__attribute__((weak)) uint32_t transport_bt_test()
{
    return 0;
}