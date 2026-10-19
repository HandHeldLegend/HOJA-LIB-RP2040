#include "cores/core_ns2gc.h"

#if defined(HOJA_NS2)

#include <stdio.h>
#include <string.h>

#include "transport/transport.h"
#include "input/mapper.h"
#include "input/hover.h"
#include "input/dpad.h"

#include "ns2_lib.h"
#include "ns2_lib_usb.h"

// How this build answers the console's pairing commands (ns2_usb_pairing_t)
#ifndef HOJA_NS2_USB_PAIRING
#define HOJA_NS2_USB_PAIRING NS2_USB_PAIRING_NORMAL
#endif

#define NS2GC_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

static bool _ns2gc_connected = false;

static void _ns2gc_set_connected(bool connected)
{
    if (connected == _ns2gc_connected)
        return;
    _ns2gc_connected = connected;

    transport_evt_cb((tp_evt_s){
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {
            .connection = connected ? TP_CONNECTION_CONNECTED : TP_CONNECTION_DISCONNECTED,
        },
    });
}

//--------------------------------------------------------------------+
// NS2-LIB-HID hooks
//--------------------------------------------------------------------+

// The GameCube layout from the GameCube profile; the buttons it has no place for (ZL, C, Home,
// Capture) straight from the hardware
void ns2_api_hook_get_input(ns2_input_s *out)
{
    mapper_input_s input = mapper_get_input();
    mapper_input_s raw = {0};
    hover_access_safe(&raw);

    out->a = input.presses[GAMECUBE_CODE_A];
    out->b = input.presses[GAMECUBE_CODE_B];
    out->x = input.presses[GAMECUBE_CODE_X];
    out->y = input.presses[GAMECUBE_CODE_Y];
    out->zr = input.presses[GAMECUBE_CODE_Z];
    out->l = input.presses[GAMECUBE_CODE_L];
    out->r = input.presses[GAMECUBE_CODE_R];
    out->plus = input.presses[GAMECUBE_CODE_START];

    bool dpad[4] = {input.presses[GAMECUBE_CODE_DOWN], input.presses[GAMECUBE_CODE_RIGHT],
                    input.presses[GAMECUBE_CODE_LEFT], input.presses[GAMECUBE_CODE_UP]};
    dpad_translate_input(dpad);
    out->down = dpad[0];
    out->right = dpad[1];
    out->left = dpad[2];
    out->up = dpad[3];

    out->zl = raw.presses[INPUT_CODE_LB];
    out->c = raw.presses[INPUT_CODE_SELECT];
    out->home = raw.presses[INPUT_CODE_HOME];
    out->capture = raw.presses[INPUT_CODE_SHARE];

    // Our own calibration, reported over the whole 12-bit range (the controller's calibration
    // blocks describe the same)
    int lx = mapper_joystick_concat(0, input.inputs[GAMECUBE_CODE_LX_LEFT], input.inputs[GAMECUBE_CODE_LX_RIGHT]);
    int ly = mapper_joystick_concat(0, input.inputs[GAMECUBE_CODE_LY_DOWN], input.inputs[GAMECUBE_CODE_LY_UP]);
    int rx = mapper_joystick_concat(0, input.inputs[GAMECUBE_CODE_RX_LEFT], input.inputs[GAMECUBE_CODE_RX_RIGHT]);
    int ry = mapper_joystick_concat(0, input.inputs[GAMECUBE_CODE_RY_DOWN], input.inputs[GAMECUBE_CODE_RY_UP]);
    out->ls_x = (uint16_t)NS2GC_CLAMP(2048 + lx, 1, 4095);
    out->ls_y = (uint16_t)NS2GC_CLAMP(2048 + ly, 1, 4095);
    out->rs_x = (uint16_t)NS2GC_CLAMP(2048 + rx, 1, 4095);
    out->rs_y = (uint16_t)NS2GC_CLAMP(2048 + ry, 1, 4095);

    // Pressed through the click reads full, as on the retail controller
    out->lt = out->l ? 255 : (uint8_t)NS2GC_CLAMP(input.inputs[GAMECUBE_CODE_L_ANALOG] >> 4, 0, 255);
    out->rt = out->r ? 255 : (uint8_t)NS2GC_CLAMP(input.inputs[GAMECUBE_CODE_R_ANALOG] >> 4, 0, 255);
}

void ns2_api_hook_set_led(uint8_t led_mask, bool flashing)
{
    (void)flashing;
    uint8_t player = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (led_mask & (1u << i))
        {
            player = i + 1;
            break;
        }
    }
    transport_evt_cb((tp_evt_s){
        .evt = TP_EVT_PLAYERLED,
        .evt_playernumber = {.player_number = player},
    });
}

// The GameCube controller's motor is on or off (command 0x0A/0x02: 1 on, anything else off)
void ns2_api_hook_play_vibration_sample(uint8_t sample_id)
{
    uint8_t strength = (sample_id == 1) ? 255 : 0;
    transport_evt_cb((tp_evt_s){
        .evt = TP_EVT_ERMRUMBLE,
        .evt_ermrumble = {.left = strength, .right = strength, .leftbrake = 0, .rightbrake = 0},
    });
}

//--------------------------------------------------------------------+
// USB personality
//--------------------------------------------------------------------+

static int16_t _ns2gc_vendor_request(uint8_t request, uint16_t value, uint8_t *out, uint16_t out_size)
{
    return ns2_usb_vendor_request(request, value, out, out_size);
}

static void _ns2gc_command_rx(const uint8_t *data, uint16_t len)
{
    static uint8_t reply[NS2_USB_COMMAND_REPLY_MAX];
    uint16_t n = ns2_usb_process_command(data, len, reply, sizeof(reply));
    if (n)
        hhl_tusb_ns2_reply(reply, n);

    // The console initialised the controller: it's connected
    if (len >= 4 && data[0] == 0x03 && data[3] == 0x0D)
        _ns2gc_set_connected(true);
}

static void _ns2gc_reset(void)
{
    ns2_usb_reset();
}

void core_ns2gc_usb_config(hhl_tusb_config_s *tcfg)
{
    tcfg->driver = HHL_TUSB_DRIVER_NS2;
    ns2_usb_get_descriptor_params(&tcfg->ns2.device_descriptor, &tcfg->ns2.config_descriptor,
                                  &tcfg->ns2.config_descriptor_len, &tcfg->ns2.report_descriptor,
                                  &tcfg->ns2.report_descriptor_len);
    tcfg->ns2.vendor_request = _ns2gc_vendor_request;
    tcfg->ns2.command_rx = _ns2gc_command_rx;
    tcfg->ns2.reset = _ns2gc_reset;
}

//--------------------------------------------------------------------+
// Core
//--------------------------------------------------------------------+

static bool _ns2gc_report(core_report_s *out)
{
    uint16_t len = 0;
    if (!ns2_usb_generate_inputreport(out->data, &len))
        return false;
    out->reportformat = CORE_REPORTFORMAT_SLIPPI;
    out->size = (uint8_t)len;
    return true;
}

static void _ns2gc_tunnel(const uint8_t *data, uint16_t len)
{
    // HID output reports (0x03 at start-up): nothing known to act on yet
    (void)data;
    (void)len;
}

static void _ns2gc_transport_stop(void)
{
    _ns2gc_set_connected(false);
}

bool core_ns2gc_init(core_params_s *params)
{
    if (params->transport_type != GAMEPAD_TRANSPORT_USB)
        return false;

    ns2_device_config_s cfg = {0};
    cfg.type = NS2_DEVTYPE_GC;
    cfg.transport = NS2_TRANSPORT_USB;
    memcpy(cfg.device_mac, params->transport_dev_mac, 6);
    // A serial in the retail form (four letters, ten digits), the digits from this controller's address
    uint32_t id = ((uint32_t)params->transport_dev_mac[2] << 24) | ((uint32_t)params->transport_dev_mac[3] << 16) |
                  ((uint32_t)params->transport_dev_mac[4] << 8) | params->transport_dev_mac[5];
    snprintf((char *)cfg.device_serial, sizeof(cfg.device_serial), "HJW5%010lu", (unsigned long)id);
    memset(&cfg.colors, 0xFF, sizeof(cfg.colors));
    cfg.usb_pairing = HOJA_NS2_USB_PAIRING;

    if (ns2_api_init(&cfg) != NS2_CONFIG_OK)
        return false;

    // The retail controller reports every 4 ms
    params->core_pollrate_us = 4000;
    params->core_report_generator = _ns2gc_report;
    params->core_report_tunnel = _ns2gc_tunnel;
    params->core_transport_stop = _ns2gc_transport_stop;

    return transport_init(params);
}

#else

bool core_ns2gc_init(core_params_s *params)
{
    (void)params;
    return false;
}

void core_ns2gc_usb_config(hhl_tusb_config_s *tcfg)
{
    (void)tcfg;
}

#endif
