#include "cores/core_gamecube.h"
#include "transport/transport.h"

#include "input/mapper.h"
#include "input/dpad.h"

#define CORE_GC_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

void _core_gamecube_report_tunnel_cb(const uint8_t *data, uint16_t len)
{
    // Unused
}

bool _core_gamecube_get_generated_report(core_report_s *out)
{
    out->reportformat=CORE_REPORTFORMAT_GAMECUBE;
    out->size=CORE_GAMECUBE_REPORT_SIZE;

    core_gamecube_report_s *data = (core_gamecube_report_s*)out->data;
    mapper_input_s input = mapper_get_input();

    data->blank_2 = 1;
    data->a = input.presses[GAMECUBE_CODE_A];
    data->b = input.presses[GAMECUBE_CODE_B];
    data->x = input.presses[GAMECUBE_CODE_X];
    data->y = input.presses[GAMECUBE_CODE_Y];

    data->start = input.presses[GAMECUBE_CODE_START];
    data->l = input.presses[GAMECUBE_CODE_L];
    data->r = input.presses[GAMECUBE_CODE_R];

    bool dpad[4] = {input.presses[GAMECUBE_CODE_DOWN], input.presses[GAMECUBE_CODE_RIGHT],
                    input.presses[GAMECUBE_CODE_LEFT], input.presses[GAMECUBE_CODE_UP]};

    dpad_translate_input(dpad);

    data->dpad_down   = dpad[0];
    data->dpad_right  = dpad[1];
    data->dpad_left   = dpad[2];
    data->dpad_up     = dpad[3];

    data->z = input.presses[GAMECUBE_CODE_Z];

    // Analog stick data conversion
    const float target_max = 110.0f / 2048.0f;
    float lx = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_LX_LEFT],input.inputs[GAMECUBE_CODE_LX_RIGHT] ) * target_max;
    float ly = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_LY_DOWN],input.inputs[GAMECUBE_CODE_LY_UP]    ) * target_max;
    float rx = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_RX_LEFT],input.inputs[GAMECUBE_CODE_RX_RIGHT] ) * target_max;
    float ry = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_RY_DOWN],input.inputs[GAMECUBE_CODE_RY_UP]    ) * target_max;

    uint8_t lx8 = CORE_GC_CLAMP(lx + 128, 0, 255);
    uint8_t ly8 = CORE_GC_CLAMP(ly + 128, 0, 255);
    uint8_t rx8 = CORE_GC_CLAMP(rx + 128, 0, 255);
    uint8_t ry8 = CORE_GC_CLAMP(ry + 128, 0, 255);
    // End analog stick conversion section

    // Trigger with SP function conversion
    uint8_t lt8 = data->l ? 255 : CORE_GC_CLAMP(input.inputs[GAMECUBE_CODE_L_ANALOG] >> 4, 0, 255);
    uint8_t rt8 = data->r ? 255 : CORE_GC_CLAMP(input.inputs[GAMECUBE_CODE_R_ANALOG] >> 4, 0, 255);

    data->stick_left_x  = lx8;
    data->stick_left_y  = ly8;
    data->stick_right_x = rx8;
    data->stick_right_y = ry8;

    data->analog_trigger_l = lt8;
    data->analog_trigger_r = rt8;

    return true;
}

/*------------------------------------------------*/

// Public Functions
bool core_gamecube_init(core_params_s *params)
{
    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_JOYBUSGC:
        params->core_pollrate_us = 1000;
        break;

        case GAMEPAD_TRANSPORT_WLAN:
        params->core_pollrate_us = 2000;
        break;

        // Unsupported transport methods
        default:
        return false;
    }
    
    params->core_report_format       = CORE_REPORTFORMAT_GAMECUBE;
    params->core_report_generator    = _core_gamecube_get_generated_report;
    params->core_report_tunnel       = _core_gamecube_report_tunnel_cb;

    return transport_init(params);
}