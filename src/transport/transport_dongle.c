#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

#include "transport/transport_dongle.h"

__attribute__((weak)) void transport_dongle_stop()
{
}

__attribute__((weak)) bool transport_dongle_init(core_params_s *params)
{
    (void)params;
    return false;
}

__attribute__((weak)) void transport_dongle_task(uint64_t timestamp)
{
    (void) timestamp;
}

__attribute__((weak)) bool transport_dongle_take_mode(core_reportformat_t *format)
{
    (void)format;
    return false;
}

__attribute__((weak)) bool transport_dongle_choosing(void)
{
    return false;
}

__attribute__((weak)) uint8_t transport_dongle_static_supported(void)
{
    return 0;
}

__attribute__((weak)) uint8_t transport_dongle_static_part_status(void)
{
    return TRANSPORT_WIRELESS_PART_NA;
}
