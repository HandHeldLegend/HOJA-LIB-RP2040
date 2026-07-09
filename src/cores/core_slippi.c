#include "cores/core_slippi.h"

#include <hoja_usb.h>
#include "cores/cores.h"
#include "transport/transport.h"
#include "input/mapper.h"
#include "input/dpad.h"

// USB device/config/HID descriptors are owned by the HHL-TINYUSB-DRIVERS library
#include "hhl_tusb_slippi.h"

#define CORE_SLIPPI_CLAMP(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

static bool _slippi_connected = false;

static void _core_slippi_set_connected(bool connected)
{
    if(connected == _slippi_connected)
        return;

    _slippi_connected = connected;

    transport_evt_cb((tp_evt_s){
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {
            .connection = connected ? TP_CONNECTION_CONNECTED : TP_CONNECTION_DISCONNECTED,
        },
    });

    transport_evt_cb((tp_evt_s){
        .evt = TP_EVT_PLAYERLED,
        .evt_playernumber = {.player_number = connected ? 1u : 0u},
    });
}

static void _core_slippi_heartbeat(void)
{
    _core_slippi_set_connected(true);
}

static void _core_slippi_transport_stop(void)
{
    _core_slippi_set_connected(false);
}

// Descriptor pointers are populated from the driver library at init time.
static core_hid_device_t _slippi_hid_device = {
    .config_descriptor          = NULL,
    .config_descriptor_len      = 0,
    .hid_report_descriptor      = NULL,
    .hid_report_descriptor_len  = 0,
    .device_descriptor          = NULL,
    .pid = HHL_TUSB_SLIPPI_PID,
    .vid = HHL_TUSB_SLIPPI_VID,
    .name = "GameCube Adapter",
};

void _core_slippi_report_tunnel_cb(const uint8_t *data, uint16_t len)
{
    if(len<2) return;

    _core_slippi_heartbeat();

    uint8_t report_id = data[0];

    switch(report_id)
    {
        // Rumble Event
        case 0x11:
        uint8_t strength = (data[1] & 0x1) ? 255 : 0;
        uint8_t brake = 0;

        tp_evt_s rumble = {
            .evt = TP_EVT_ERMRUMBLE,
            .evt_ermrumble = {
            .left=strength, .right=strength,
            .leftbrake=brake, .rightbrake=brake
        }};

        transport_evt_cb(rumble);
        break;

        // Init adapter 
        case 0x013:
        break;

        default:
        break;
    }

}

bool _core_slippi_get_generated_report(core_report_s *out)
{
    const uint8_t report_id = 0x21;
    static bool _slippi_first = false;

    out->reportformat = CORE_REPORTFORMAT_SLIPPI;
    out->size = 37;

    out->data[0] = report_id;

    /*GC adapter notes
    with only black USB plugged in
    - no controller, byte 1 is 0
    - controller plugged in to port 1, byte 1 is 0x10
    - controller plugged in port 2, byte 10 is 0x10
    with both USB plugged in
    - no controller, byte 1 is 0x04
    - controller plugged in to port 1, byte is 0x14 */
    out->data[1]  = 0x14;
    out->data[10] = 0x04;
    out->data[19] = 0x04;
    out->data[28] = 0x04;

    // Do not populate any data on first report
    if(!_slippi_first)
    {
        _slippi_first = true;
        return true;
    }

    core_slippi_report_s *data = (core_slippi_report_s*)&out->data[2];
    mapper_input_s input = mapper_get_input();

    const float   target_max = 110.0f / 2048.0f;
    float lx = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_LX_LEFT],input.inputs[GAMECUBE_CODE_LX_RIGHT] ) * target_max;
    float ly = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_LY_DOWN],input.inputs[GAMECUBE_CODE_LY_UP]    ) * target_max;
    float rx = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_RX_LEFT],input.inputs[GAMECUBE_CODE_RX_RIGHT] ) * target_max;
    float ry = mapper_joystick_concat(0,input.inputs[GAMECUBE_CODE_RY_DOWN],input.inputs[GAMECUBE_CODE_RY_UP]    ) * target_max;

    uint8_t lx8 = (uint8_t)CORE_SLIPPI_CLAMP(lx + 128);
    uint8_t ly8 = (uint8_t)CORE_SLIPPI_CLAMP(ly + 128);
    uint8_t rx8 = (uint8_t)CORE_SLIPPI_CLAMP(rx + 128);
    uint8_t ry8 = (uint8_t)CORE_SLIPPI_CLAMP(ry + 128);

    data->button_a = input.presses[GAMECUBE_CODE_A];
    data->button_b = input.presses[GAMECUBE_CODE_B];
    data->button_x = input.presses[GAMECUBE_CODE_X];
    data->button_y = input.presses[GAMECUBE_CODE_Y];

    data->button_start   = input.presses[GAMECUBE_CODE_START];
    data->button_l       = input.presses[GAMECUBE_CODE_L];
    data->button_r       = input.presses[GAMECUBE_CODE_R];

    uint8_t lt8 = data->button_l ? 255 : CORE_SLIPPI_CLAMP(input.inputs[GAMECUBE_CODE_L_ANALOG] >> 4);
    uint8_t rt8 = data->button_r ? 255 : CORE_SLIPPI_CLAMP(input.inputs[GAMECUBE_CODE_R_ANALOG] >> 4);

    bool dpad[4] = {input.presses[GAMECUBE_CODE_DOWN], input.presses[GAMECUBE_CODE_RIGHT],
                    input.presses[GAMECUBE_CODE_LEFT], input.presses[GAMECUBE_CODE_UP]};

    dpad_translate_input(dpad);

    data->dpad_down   = dpad[0];
    data->dpad_right  = dpad[1];
    data->dpad_left   = dpad[2];
    data->dpad_up     = dpad[3];
    
    data->button_z    = input.presses[GAMECUBE_CODE_Z];

    data->stick_x = lx8;
    data->stick_y = ly8;
    data->cstick_x = rx8;
    data->cstick_y = ry8;
    data->trigger_l  = lt8;
    data->trigger_r  = rt8;

    _core_slippi_heartbeat();

    return true;
}

/*------------------------------------------------*/

static void _core_slippi_populate_hid_device(void)
{
    _slippi_hid_device.device_descriptor         =
        (const hoja_usb_device_descriptor_t *)hhl_tusb_slippi_device_descriptor();
    _slippi_hid_device.config_descriptor         = hhl_tusb_slippi_configuration_descriptor();
    _slippi_hid_device.config_descriptor_len     = hhl_tusb_slippi_configuration_descriptor_len();
    _slippi_hid_device.hid_report_descriptor     = hhl_tusb_slippi_hid_report_descriptor();
    _slippi_hid_device.hid_report_descriptor_len = hhl_tusb_slippi_hid_report_descriptor_len();
}

// Public Functions
bool core_slippi_init(core_params_s *params)
{
    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_USB:
        params->core_pollrate_us = 1000;
        break;

        case GAMEPAD_TRANSPORT_WLAN:
        params->core_pollrate_us = 2000;
        break;

        // Unsupported transport methods
        default:
        return false;
    }

    _core_slippi_populate_hid_device();
    params->hid_device = &_slippi_hid_device;
    
    params->core_report_format       = CORE_REPORTFORMAT_SLIPPI;
    params->core_report_generator    = _core_slippi_get_generated_report;
    params->core_report_tunnel       = _core_slippi_report_tunnel_cb;
    params->core_transport_stop      = _core_slippi_transport_stop;

    return transport_init(params);
}