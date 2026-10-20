#include "cores/core_ns2gc.h"

#if defined(HOJA_NS2)

#include <stdio.h>
#include <string.h>

#include "transport/transport.h"
#include "input/mapper.h"
#include "input/hover.h"
#include "input/dpad.h"
#include "hal/sys_hal.h"

#include "ns2_lib.h"
#include "ns2_lib_usb.h"

// How this build answers the console's pairing commands (ns2_usb_pairing_t)
#ifndef HOJA_NS2_USB_PAIRING
#define HOJA_NS2_USB_PAIRING NS2_USB_PAIRING_NORMAL
#endif

#define NS2GC_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

static bool _ns2gc_connected = false;

// The USB transport starts only for a mode with a device record (the driver takes the
// descriptors from core_ns2gc_usb_config)
static core_hid_device_t _ns2gc_hid_device = {
    .vid = 0x057E,
    .pid = 0x2073,
    .name = "GameCube Controller",
};

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

// The GameCube layout from the GameCube profile; the rest from the hardware, on their Pro
// Controller 2 bits (the report shares its layout): ZL, Minus, the stick clicks, Home, Capture,
// and C (chat) on the power button
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
    out->minus = raw.presses[INPUT_CODE_SELECT];
    out->l_js = raw.presses[INPUT_CODE_LS];
    out->r_js = raw.presses[INPUT_CODE_RS];
    out->home = raw.presses[INPUT_CODE_HOME];
    out->capture = raw.presses[INPUT_CODE_SHARE];
    out->c = raw.presses[INPUT_CODE_MISC3];

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

//--------------------------------------------------------------------+
// Rumble: the GameCube controller has one motor, on or off. Games drive it with output report
// 0x03; the console's own sounds are samples it asks for by ID (command 0x0A/0x02).
//--------------------------------------------------------------------+

static bool _ns2gc_game_rumble = false;
static uint8_t _ns2gc_motor = 0;

// A sample as motor steps: on, off, on, ... in ms, ending at 0. The retail controller's: 1 a
// low buzz of about a second, 2 the "search for controllers" alarm, 3 the soft click-click of a
// connection, 4..7 short beeps.
static const uint16_t _ns2gc_sample_steps[8][6] = {
    {0},
    {1000, 0},
    {300, 100, 120, 80, 120, 0},
    {60, 80, 60, 0},
    {120, 80, 120, 0},
    {90, 80, 90, 0},
    {80, 0},
    {60, 0},
};
static const uint16_t *_ns2gc_sample = NULL;
static uint8_t _ns2gc_sample_step = 0;
static uint64_t _ns2gc_sample_until_us = 0;

static void _ns2gc_motor_set(uint8_t strength)
{
    if (strength == _ns2gc_motor)
        return;
    _ns2gc_motor = strength;
    transport_evt_cb((tp_evt_s){
        .evt = TP_EVT_ERMRUMBLE,
        .evt_ermrumble = {.left = strength, .right = strength, .leftbrake = 0, .rightbrake = 0},
    });
}

// Steps a playing sample and sets the motor: a game's rumble, else the sample's step
static void _ns2gc_rumble_task(void)
{
    uint64_t now = sys_hal_now_us();
    while (_ns2gc_sample && now >= _ns2gc_sample_until_us)
    {
        _ns2gc_sample_step++;
        uint16_t ms = _ns2gc_sample[_ns2gc_sample_step];
        if (_ns2gc_sample_step >= 6 || !ms)
        {
            _ns2gc_sample = NULL;
            break;
        }
        _ns2gc_sample_until_us = now + (uint64_t)ms * 1000u;
    }

    // Even steps are on, odd steps off
    bool sample_on = _ns2gc_sample && !(_ns2gc_sample_step & 1);
    _ns2gc_motor_set((_ns2gc_game_rumble || sample_on) ? 255 : 0);
}

// Command 0x0A/0x02: play a sample (0 stops the one playing)
void ns2_api_hook_play_vibration_sample(uint8_t sample_id)
{
    if (sample_id == 0 || sample_id >= 8)
    {
        _ns2gc_sample = NULL;
    }
    else
    {
        _ns2gc_sample = _ns2gc_sample_steps[sample_id];
        _ns2gc_sample_step = 0;
        _ns2gc_sample_until_us = sys_hal_now_us() + (uint64_t)_ns2gc_sample[0] * 1000u;
    }
    _ns2gc_rumble_task();
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
    _ns2gc_rumble_task();

    uint16_t len = 0;
    if (!ns2_usb_generate_inputreport(out->data, &len))
        return false;
    out->reportformat = CORE_REPORTFORMAT_SLIPPI;
    out->size = (uint8_t)len;
    return true;
}

// Output report 0x03 from a game: byte 1 counts in its low nibble, then the GameCube's rumble
// state as 2-bit fields, rumble (low bit) and brake (high bit), as the real GameCube has it. Any
// rumble bit runs the motor; brake or nothing stops it. Seen: 51 02 00 00 brake (games send it to
// stop), 61 00 01 00 rumble, 5B 00 00 00 stop.
static void _ns2gc_tunnel(const uint8_t *data, uint16_t len)
{
    if (len < 5 || data[0] != 0x03)
        return;
    _ns2gc_game_rumble = ((data[2] | data[3] | data[4]) & 0x55) != 0;
    _ns2gc_rumble_task();
}

static void _ns2gc_transport_stop(void)
{
    _ns2gc_game_rumble = false;
    _ns2gc_sample = NULL;
    _ns2gc_motor_set(0);
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
    params->hid_device = &_ns2gc_hid_device;

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
