#include <stdlib.h>

#include "cores/core_n64.h"
#include "transport/transport.h"

#include "board_config.h"

#include "input/mapper.h"
#include "input/dpad.h"

#include "hoja_shared_types.h"
#include "hoja.h"

#include <string.h>

// Generic fallback name; identity (name/vid/pid) is overridden at init from the
// board's hoja config.
#define CORE_N64_WLAN_NAME "N64 Controller"

static core_hid_device_t _n64_wlan_hid = {
    .vid  = 0,
    .pid  = 0,
    .name = CORE_N64_WLAN_NAME,
};

#define CORE_N64_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

// Callback function for our transport to obtain the latest report
bool _core_n64_get_generated_report(core_report_s *out)
{
    out->reportformat=CORE_REPORTFORMAT_N64;
    out->size=CORE_N64_REPORT_SIZE;

    core_n64_report_s *data = (core_n64_report_s*)out->data;
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

    return true;
}

/*------------------------------------------------*/

// Public Functions
bool core_n64_init(core_params_s *params)
{
    switch(params->transport_type)
    {
        // Supported transport methods
        case GAMEPAD_TRANSPORT_JOYBUS64:
        params->core_pollrate_us = 1000;
        break;

        case GAMEPAD_TRANSPORT_WLAN:
        params->core_pollrate_us = 2000;
        {
            const hoja_config_s *cfg = hoja_config_get();
            if(cfg && cfg->device_name)
            {
                memset(_n64_wlan_hid.name, 0, sizeof(_n64_wlan_hid.name));
                strncpy(_n64_wlan_hid.name, cfg->device_name, sizeof(_n64_wlan_hid.name) - 1u);
            }
            if(cfg) _n64_wlan_hid.vid = cfg->usb_vid;
            if(cfg) _n64_wlan_hid.pid = cfg->usb_pid;
        }
        params->hid_device = &_n64_wlan_hid;
        break;

        // Unsupported transport methods
        default:
        return false;
    }
    
    params->core_report_format      = CORE_REPORTFORMAT_N64;
    params->core_report_generator   = _core_n64_get_generated_report;
    params->core_report_tunnel      = NULL;

    return transport_init(params);
}
