#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cores/cores.h"
#include "transport/transport.h"

#include "hal/sys_hal.h"

#include "cores/core_switch.h"
#include "cores/core_sinput.h"
#include "cores/core_xinput.h"
#include "cores/core_slippi.h"

#include "cores/core_snes.h"
#include "cores/core_n64.h"
#include "cores/core_gamecube.h"

#include "devices/battery.h"
#include "utilities/settings.h"

const core_params_s _core_params_default = {
    .core_report_format = CORE_REPORTFORMAT_UNDEFINED,
    .core_pollrate_us = 8000,
    .core_report_generator = NULL,
    .core_report_tunnel = NULL,

    .transport_type = GAMEPAD_TRANSPORT_UNDEFINED,
    .transport_dev_mac = {0,0,0,0,0,0},
    .transport_host_mac = {0,0,0,0,0,0},
    .transport_task = NULL,

    .hid_device = NULL,
};

core_params_s _core_params = {
    .core_report_format = CORE_REPORTFORMAT_UNDEFINED,
    .core_pollrate_us = 8000,
    .core_report_generator = NULL,
    .core_report_tunnel = NULL,

    .transport_type = GAMEPAD_TRANSPORT_UNDEFINED,
    .transport_dev_mac = {0,0,0,0,0,0},
    .transport_host_mac = {0,0,0,0,0,0},
    .transport_task = NULL,

    .hid_device = NULL,
};

// Runs auto-detect procedures
// and will call the callback with the resulting gamepad
// mode that is detected (or -1 if we didn't have a success)
void core_detect_init(core_autodetect_result_t cb)
{

}

bool core_is_mac_blank(uint8_t mac[6])
{
    for(int i=0; i<6; i++)
    {
        if(mac[i]>0) return false;
    }
    
    return true;
}

bool core_get_generated_report(core_report_s *out)
{
    if(!_core_params.core_report_generator) return false;
    return _core_params.core_report_generator(out);
}

void core_report_tunnel_cb(const uint8_t *data, uint16_t len)
{
    if(!_core_params.core_report_tunnel) return;
    _core_params.core_report_tunnel(data, len);
}

core_params_s* core_current_params()
{
    return &_core_params;
}

bool core_init(core_reportformat_t format, gamepad_transport_t transport, bool pair, uint16_t boot_flags)
{
    _core_params.transport_type = transport;
    _core_params.core_boot_flags = boot_flags;

    if (pair)
        _core_params.core_boot_flags |= COREBOOT_FLAG_PAIR;

    // Clear host mac just in case first
    memset(_core_params.transport_host_mac, 0, 6);

    // Copy in device MAC
    memcpy(_core_params.transport_dev_mac, gamepad_config->gamepad_mac_address, 6);

    switch(transport)
    {
        case GAMEPAD_TRANSPORT_USB:
        battery_set_charge_rate(200);
        break;


        case GAMEPAD_TRANSPORT_WLAN:
        case GAMEPAD_TRANSPORT_BLUETOOTH:
        battery_set_charge_rate(250);
        break;

        case GAMEPAD_TRANSPORT_NESBUS:
        case GAMEPAD_TRANSPORT_JOYBUSGC:
        case GAMEPAD_TRANSPORT_JOYBUS64:
        battery_set_charge_rate(0);
        break;

        // Unsupported transport mode
        default:
        return false;
    }

    switch(format)
    {
        case CORE_REPORTFORMAT_SWPRO:
        return core_switch_init(&_core_params);

        case CORE_REPORTFORMAT_XINPUT:
        return core_xinput_init(&_core_params);

        case CORE_REPORTFORMAT_SINPUT:
        return core_sinput_init(&_core_params);

        case CORE_REPORTFORMAT_SNES:
        return core_snes_init(&_core_params);

        case CORE_REPORTFORMAT_N64:
        return core_n64_init(&_core_params);

        case CORE_REPORTFORMAT_GAMECUBE:
        return core_gamecube_init(&_core_params);

        case CORE_REPORTFORMAT_SLIPPI:
        return core_slippi_init(&_core_params);

        default:
        return false;
    }
}

void core_deinit()
{
    _core_params.transport_task = NULL;
    transport_stop();
}

void core_task(uint64_t now_us)
{
    if(!_core_params.transport_task) return;
    _core_params.transport_task(sys_hal_now_us());
}