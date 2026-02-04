#include "cores/core_n64.h"
#include "transport/transport.h"

#include "input/mapper.h"
#include "input/dpad.h"

#include "hoja_shared_types.h"
#include "utilities/crosscore_snapshot.h"

SNAPSHOT_TYPE(n64report, core_report_s);

snapshot_n64report_t _n64_snapshot;

#define CORE_N64_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

// Obtain and remap our inputs
void _core_n64_set_input()
{
    core_report_s report    = {.reportformat=CORE_REPORTFORMAT_N64, .size=CORE_N64_REPORT_SIZE};
    core_n64_report_s *data = (core_n64_report_s*)report.data;
    mapper_input_s input = mapper_get_input();

    data->button_a = input.presses[N64_CODE_A];
    data->button_b = input.presses[N64_CODE_B];

    data->cpad_up = input.presses[N64_CODE_CUP];
    data->cpad_down = input.presses[N64_CODE_CDOWN];

    data->cpad_left = input.presses[N64_CODE_CLEFT];
    data->cpad_right = input.presses[N64_CODE_CRIGHT];

    data->button_start = input.presses[N64_CODE_START];

    data->button_l = input.presses[N64_CODE_L];

    data->button_z = input.presses[N64_CODE_Z];
    data->button_r = input.presses[N64_CODE_R];

    const float target_max = 85.0f / 2048.0f;
    float lx = mapper_joystick_concat(0, input.inputs[N64_CODE_LX_LEFT], input.inputs[N64_CODE_LX_RIGHT]) * target_max;
    float ly = mapper_joystick_concat(0, input.inputs[N64_CODE_LY_DOWN], input.inputs[N64_CODE_LY_UP]) * target_max;

    int8_t lx8 = CORE_N64_CLAMP(lx, -128, 127);
    int8_t ly8 = CORE_N64_CLAMP(ly, -128, 127);

    data->stick_x = lx8;
    data->stick_y = ly8;

    bool dpad[4] = {input.presses[N64_CODE_DOWN], input.presses[N64_CODE_RIGHT],
                    input.presses[N64_CODE_LEFT], input.presses[N64_CODE_UP]};

    dpad_translate_input(dpad);

    data->dpad_down = dpad[0];
    data->dpad_right = dpad[1];
    data->dpad_left = dpad[2];
    data->dpad_up = dpad[3];

    snapshot_n64report_write(&_n64_snapshot, &report);
}

// Callback function for our transport to obtain the latest report
bool _core_n64_get_generated_report(core_report_s *out)
{
    snapshot_n64report_read(&_n64_snapshot, out);
    return true;
}

/*------------------------------------------------*/

// Public Functions
bool core_n64_init(core_params_s *params)
{
    switch(params->gamepad_transport)
    {
        // Supported transport methods
        case GAMEPAD_TRANSPORT_JOYBUS64:
        case GAMEPAD_TRANSPORT_WLAN:
        break;

        // Unsupported transport methods
        default:
        return false;
    }

    params->report_generator = _core_n64_get_generated_report;
    params->report_tunnel = NULL;

    return transport_init(params);
}

void core_n64_task(uint64_t timestamp)
{
    
}