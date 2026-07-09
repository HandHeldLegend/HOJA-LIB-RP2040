#include "cores/core_snes.h"

#include "transport/transport.h"
#include "input/mapper.h"
#include "input/dpad.h"

void _core_snes_report_tunnel_cb(const uint8_t *data, uint16_t len)
{
    // Unused
}

bool _core_snes_get_generated_report(core_report_s *out)
{
    out->reportformat=CORE_REPORTFORMAT_SNES;
    out->size = CORE_SNES_REPORT_SIZE;
    
    core_snes_report_s *data = (core_snes_report_s*)out->data;

    mapper_input_s input = mapper_get_input();

    bool dpad[4] = {input.presses[SNES_CODE_DOWN], input.presses[SNES_CODE_RIGHT],
                    input.presses[SNES_CODE_LEFT], input.presses[SNES_CODE_UP]};

    dpad_translate_input(dpad);

    data->a = !input.presses[SNES_CODE_A];
    data->b = !input.presses[SNES_CODE_B];
    data->x = !input.presses[SNES_CODE_X];
    data->y = !input.presses[SNES_CODE_Y];

    data->l = !input.presses[SNES_CODE_L];
    data->r = !input.presses[SNES_CODE_R];

    data->start = !input.presses[SNES_CODE_START];
    data->select = !input.presses[SNES_CODE_SELECT];

    data->ddown  = !dpad[0];
    data->dright = !dpad[1];
    data->dleft  = !dpad[2];
    data->dup    = !dpad[3];

    return true;
}

/*------------------------------------------------*/

// Public Functions
bool core_snes_init(core_params_s *params)
{
    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_NESBUS:
        params->core_pollrate_us = 1000;
        break;

        // Unsupported transport methods
        default:
        return false;
    }
    
    params->core_report_format       = CORE_REPORTFORMAT_SNES;
    params->core_report_generator    = _core_snes_get_generated_report;
    params->core_report_tunnel       = _core_snes_report_tunnel_cb;

    return transport_init(params);
}