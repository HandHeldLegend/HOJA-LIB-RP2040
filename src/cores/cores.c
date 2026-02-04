#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

#include "cores/core_switch.h"
#include "cores/core_sinput.h"
#include "cores/core_xinput.h"

#include "cores/core_n64.h"

core_params_s _core_params = {
    .gamepad_mode = GAMEPAD_MODE_UNDEFINED,
    .gamepad_transport = GAMEPAD_TRANSPORT_UNDEFINED,
    .report_format = CORE_REPORTFORMAT_UNDEFINED,
    .report_generator = NULL,
    .report_tunnel = NULL,
};

bool core_get_generated_report(core_report_s *out)
{
    if(!_core_params.report_generator) return false;
    return _core_params.report_generator(out);
}

void core_report_tunnel_cb(uint8_t *data, uint16_t len)
{
    if(!_core_params.report_tunnel) return;
    _core_params.report_tunnel(data, len);
}

bool core_init(gamepad_mode_t mode, gamepad_transport_t transport)
{
    _core_params.gamepad_mode = mode;
    _core_params.gamepad_transport = transport;

    switch(mode)
    {
        case GAMEPAD_MODE_SWPRO:
        break;

        case GAMEPAD_MODE_XINPUT:
        break;

        case GAMEPAD_MODE_SINPUT:
        return core_sinput_init(&_core_params);

        case GAMEPAD_MODE_SNES:
        break;

        case GAMEPAD_MODE_N64:
        return core_n64_init(&_core_params);
        break;

        case GAMEPAD_MODE_GAMECUBE:
        break;

        case GAMEPAD_MODE_GCUSB:
        break;

        default:
        return false;
    }
}

void core_task(uint64_t timestamp)
{
    if(!_core_params.transport_task) return;
    _core_params.transport_task(timestamp);
}