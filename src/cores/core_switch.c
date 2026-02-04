#include "cores/core_switch.h"

#include "switch/switch_commands.h"
#include "switch/switch_haptics.h"

#include "transport/transport.h"

#define REPORT_ID_SWITCH_INPUT 0x30
#define REPORT_ID_SWITCH_CMD 0x21
#define REPORT_ID_SWITCH_INIT 0x81



#define CORE_SWITCH_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

uint8_t _switch_report_size = 64;

void _core_switch_report_tunnel_cb(uint8_t *data, uint16_t len)
{
    if(len<2) return;

    uint8_t report_id = data[0];

    switch(report_id)
    {
        case SW_OUT_ID_RUMBLE:
        switch_haptics_rumble_translate(&data[2]);
        break;

        default:
        switch_commands_future_handle(data[0], data, len);
        break;
    }
}

bool _core_switch_get_generated_report(core_report_s *out)
{
    out->reportformat=CORE_REPORTFORMAT_SWPRO;
    out->size=_switch_report_size; // Includes report ID

    // Generate switch input data
    out->data[0] = 0; // SW
    return true;
}

/*------------------------------------------------*/

// Public Functions
bool core_switch_init(core_params_s *params)
{
    switch(params->gamepad_transport)
    {
        case GAMEPAD_TRANSPORT_USB:
        _switch_report_size = 64;
        break;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        _switch_report_size = 49;
        break;

        // Unsupported transport methods
        default:
        return false;
    }

    params->report_generator = _core_switch_get_generated_report;
    params->report_tunnel = _core_switch_report_tunnel_cb;

    return transport_init(params);
}