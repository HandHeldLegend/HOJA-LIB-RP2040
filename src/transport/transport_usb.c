#include "transport/transport_usb.h"

__attribute__((weak)) void transport_usb_stop()
{
    
}

__attribute__((weak)) bool transport_usb_init(core_params_s *params)
{
    return false;
}

__attribute__((weak)) void transport_usb_task(uint64_t timestamp)
{
    (void) timestamp;
}