#include <string.h>

#include "cores/core_switch.h"

#include "switch/switch_commands.h"
#include "switch/switch_haptics.h"

#include "transport/transport.h"

#define REPORT_ID_SWITCH_INPUT 0x30
#define REPORT_ID_SWITCH_CMD 0x21
#define REPORT_ID_SWITCH_INIT 0x81



#define CORE_SWITCH_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

ns_command_data_s _scmd = {0};
uint8_t _switch_report_size = 64;

void _core_switch_report_tunnel_cb(uint8_t *data, uint16_t len)
{
    if(len<2) return;

    uint8_t report_id = data[0];

    switch(report_id)
    {
        // If we only have rumble, we process and move on
        case SW_OUT_ID_RUMBLE:
        switch_haptics_rumble_translate(&data[2]);
        break;

        case SW_OUT_ID_RUMBLE_CMD:
        // Process rumble immediately, save data to process command
        // response on next packet generation
        switch_haptics_rumble_translate(&data[2]);
        // Fall through to default
        default:
        // Write to our ns_command_data_s struct
        _scmd.cmd_report_id = report_id;
        memcpy(_scmd.cmd_data, data, len-1);
        break;
    }
}

bool _core_switch_get_generated_report(core_report_s *out)
{
    out->reportformat=CORE_REPORTFORMAT_SWPRO;
    out->size=_switch_report_size; // Includes report ID

    // Flush output buffer
    memset(out->data, 0, CORE_REPORT_DATA_LEN);

    // Check if we have command data
    // that we must respond to
    if(_scmd.cmd_report_id)
    {
        swcmd_generate_reply(&_scmd, &out->data[0] ,&out->data[1]);

        // Clear command data
        _scmd.cmd_report_id=0;
        memset(_scmd.cmd_data, 0, NS_CMD_DATA_LEN);
    }
    // Just generate input data and send
    else 
    {
        // Set the input data
        core_switch_report_s data = {0};

        mapper_input_s input = mapper_get_input();

        // Apply remapped data to our output buffer
        bool dpad[4] = {input.presses[SWITCH_CODE_DOWN], input.presses[SWITCH_CODE_RIGHT],
                    input.presses[SWITCH_CODE_LEFT], input.presses[SWITCH_CODE_UP]};

        dpad_translate_input(dpad);

        data.d_down     = dpad[0];
        data.d_right    = dpad[1];
        data.d_left     = dpad[2];
        data.d_up       = dpad[3];

        data.b_y = input.presses[SWITCH_CODE_Y];
        data.b_x = input.presses[SWITCH_CODE_X];
        data.b_a = input.presses[SWITCH_CODE_A];
        data.b_b = input.presses[SWITCH_CODE_B];

        data.b_minus    = input.presses[SWITCH_CODE_MINUS];
        data.b_plus     = input.presses[SWITCH_CODE_PLUS];
        data.b_home     = input.presses[SWITCH_CODE_HOME];
        data.b_capture  = input.presses[SWITCH_CODE_CAPTURE];

        data.sb_left  = input.presses[SWITCH_CODE_LS];
        data.sb_right = input.presses[SWITCH_CODE_RS];

        data.t_r = input.presses[SWITCH_CODE_R];
        data.t_l = input.presses[SWITCH_CODE_L];  
        data.t_zl = input.presses[SWITCH_CODE_ZL];
        data.t_zr = input.presses[SWITCH_CODE_ZR];

        uint16_t lx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LX_LEFT], input.inputs[SWITCH_CODE_LX_RIGHT]);
        uint16_t ly = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LY_DOWN], input.inputs[SWITCH_CODE_LY_UP]);
        uint16_t rx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RX_LEFT], input.inputs[SWITCH_CODE_RX_RIGHT]);
        uint16_t ry = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RY_DOWN], input.inputs[SWITCH_CODE_RY_UP]);

        lx = (uint16_t) SWITCH_CLAMP(lx, 0, 4095); 
        ly = (uint16_t) SWITCH_CLAMP(ly, 0, 4095); 
        rx = (uint16_t) SWITCH_CLAMP(rx, 0, 4095); 
        ry = (uint16_t) SWITCH_CLAMP(ry, 0, 4095); 
        
        // Custom mapping of bits for output for joysticks/buttons
        out->data[1] =  data.right_buttons;
        out->data[2] =  data.shared_buttons;
        out->data[3] =  data.left_buttons;
        out->data[4] =  (lx & 0xFF);
        out->data[5] =  (lx & 0xF00) >> 8;
        out->data[5] |= (ly & 0xF) << 4;
        out->data[6] =  (ly & 0xFF0) >> 4;
        out->data[7] =  (rx & 0xFF);
        out->data[8] =  (rx & 0xF00) >> 8;
        out->data[8] |= (ry & 0xF) << 4;
        out->data[9] =  (ry & 0xFF0) >> 4;

        swcmd_generate_inputreport(&out->data[0], &out->data[1]);
    }

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