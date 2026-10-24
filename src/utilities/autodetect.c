#include "utilities/autodetect.h"

#include "hoja.h"
#include "cores/cores.h"
#include "hal/sys_hal.h"
#include "transport/transport_bt.h"
#include "transport/transport_dongle.h"
#include "transport/transport_joybus64.h"
#include "utilities/boot_timeline.h"
#include "utilities/settings.h"
#include "hhl_tusb_config.h" // Which USB modes this build has (XInput, Slippi)

#include "hardware/gpio.h"

// No joybus polls by then means no N64: a running game polls every frame
#define AUTODETECT_N64_WAIT_MS      1000
// No USB host, GameCube or SNES by then means a charger
#define AUTODETECT_WIRED_WAIT_MS    3000
// A computer asks for the BOS descriptor within milliseconds of its first request
#define AUTODETECT_BOS_WAIT_MS      500
// The host calls (or the probe) give up and let the mode carry on (a radio that never came up). Each
// saved host takes a radio restart, a 1.6 s call and up to 3 s for the answer, so real calls end
// well before.
#define AUTODETECT_BT_PROBE_MAX_MS  30000
// No dongle connected by then: look for the saved hosts. Covers a failed page of the dongle we
// remember (1.28 s), one search round for any other (2.56 s, FLOSS_LINK_INQUIRY_LENGTH) and the
// connection after it: measured 4.3 s on the GCU-2 from its radio coming up.
#define AUTODETECT_DONGLE_WAIT_MS   5500
// Edges that count as a console on the retro port
#define AUTODETECT_JOYBUS_EDGES     20
// Identify commands with no poll among them, answering as an N64: a GameCube (our N64 answer means
// nothing to it, so it keeps asking). An N64 polls on the frame after its first.
#define AUTODETECT_GAMECUBE_PROBES  3
#define AUTODETECT_LATCH_EDGES      10

// What this board can be plugged into
#if defined(HOJA_TRANSPORT_JOYBUS64_DRIVER)
#define AUTODETECT_N64 1
#else
#define AUTODETECT_N64 0
#endif

#if defined(HOJA_TRANSPORT_JOYBUSGC_DRIVER)
#define AUTODETECT_GAMECUBE 1
#else
#define AUTODETECT_GAMECUBE 0
#endif

#if defined(HOJA_TRANSPORT_NESBUS_DRIVER)
#define AUTODETECT_SNES 1
#else
#define AUTODETECT_SNES 0
#endif

#if defined(HOJA_TRANSPORT_BT_DRIVER)
#define AUTODETECT_BLUETOOTH 1
#else
#define AUTODETECT_BLUETOOTH 0
#endif

// The dongle over Bluetooth (FLOSS boards): Auto looks for it before the saved hosts
#if defined(HOJA_TRANSPORT_DONGLE_DRIVER) && defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_FLOSS
#define AUTODETECT_DONGLE 1
#else
#define AUTODETECT_DONGLE 0
#endif

typedef enum
{
    PHASE_OFF = 0,
    PHASE_N64,   // Answering as an N64 controller, waiting for polls
    PHASE_WIRED, // SInput over USB, watching USB and the retro port
    PHASE_DONGLE, // Looking for a dongle over Bluetooth; it then tells us the mode
    PHASE_BT_HOST, // Calling a saved host in its mode, the next one if it doesn't answer (FLOSS)
    PHASE_BT,    // Switch over Bluetooth, probing the saved hosts first (BTstack)
    PHASE_DONE,  // Mode picked, no more switching
} phase_t;

static volatile phase_t _phase = PHASE_OFF;
static volatile bool _confirmed = false;
static uint64_t _phase_start_us = 0;
static bool _phase_started = false;

static volatile bool _usb_host = false;
static volatile bool _usb_bos = false;
static volatile uint32_t _joybus_edges = 0;
static volatile uint32_t _latch_edges = 0;

// Mode switch, carried out between task passes on core 1
static volatile bool _switch_pending = false;
static core_reportformat_t _switch_format;
static gamepad_transport_t _switch_transport;

static bool _paused = false; // The config app holds the radio (autodetect_pause)

// Auto on USB with a computer: the boot order's choice (Steam, XInput, Slippi), or Switch for always
// Switch. A mode this board can't do over USB counts as Steam.
static core_reportformat_t _usb_computer_mode(void)
{
    const uint8_t mode = gamepad_config->auto_usb_computer;
    if(gamepad_config->auto_order_version != AUTO_ORDER_VERSION)
        return CORE_REPORTFORMAT_SINPUT;
    switch(mode)
    {
        case CORE_REPORTFORMAT_SWPRO:
        case CORE_REPORTFORMAT_SINPUT:
            return (core_reportformat_t)mode;
#if (HHL_TUSB_DRIVER_XINPUT_ENABLE)
        case CORE_REPORTFORMAT_XINPUT:
            return CORE_REPORTFORMAT_XINPUT;
#endif
#if (HHL_TUSB_DRIVER_SLIPPI_ENABLE)
        case CORE_REPORTFORMAT_SLIPPI:
            return CORE_REPORTFORMAT_SLIPPI;
#endif
        default:
            return CORE_REPORTFORMAT_SINPUT;
    }
}

// The USB mode Auto starts in while it watches: Switch when it's always Switch, else Steam (which a
// computer answers, and a Switch is moved off)
static core_reportformat_t _usb_start_mode(void)
{
    return (_usb_computer_mode() == CORE_REPORTFORMAT_SWPRO) ? CORE_REPORTFORMAT_SWPRO : CORE_REPORTFORMAT_SINPUT;
}

#if AUTODETECT_N64 && AUTODETECT_GAMECUBE
static uint32_t _n64_probes_start = 0;
static uint32_t _n64_polls_start = 0;
#endif

static void _switch_to(core_reportformat_t format, gamepad_transport_t transport, phase_t next)
{
    _switch_format = format;
    _switch_transport = transport;
    _phase = next;
    BOOT_TIMELINE(TL_PHASE, next);
    _phase_started = false;
    _switch_pending = true;
}

#if AUTODETECT_GAMECUBE || AUTODETECT_SNES
static volatile bool _joybus_watching = false;
static volatile bool _latch_watching = false;

// Every edge is acknowledged, counted only while watched: one that lands as a watch stops (between
// the flag clearing and the pin's interrupt turning off) would otherwise stay pending and hold the
// core in this handler for good. A GameCube's traffic hit that about half the time.
static void _retro_port_irq(void)
{
    const hoja_config_s *cfg = hoja_config_get();

#if AUTODETECT_GAMECUBE
    const uint data_pin = cfg->joybus.data_pin;
    if(gpio_get_irq_event_mask(data_pin) & GPIO_IRQ_EDGE_FALL)
    {
        gpio_acknowledge_irq(data_pin, GPIO_IRQ_EDGE_FALL);
        if(_joybus_watching) _joybus_edges++;
    }
#endif

#if AUTODETECT_SNES
    const uint latch_pin = cfg->nesbus.latch_pin;
    if(gpio_get_irq_event_mask(latch_pin) & GPIO_IRQ_EDGE_RISE)
    {
        gpio_acknowledge_irq(latch_pin, GPIO_IRQ_EDGE_RISE);
        if(_latch_watching) _latch_edges++;
    }
#endif
}

static uint32_t _retro_port_mask(const hoja_config_s *cfg)
{
    uint32_t mask = 0;
#if AUTODETECT_GAMECUBE
    mask |= (1u << cfg->joybus.data_pin);
#endif
#if AUTODETECT_SNES
    mask |= (1u << cfg->nesbus.latch_pin);
#endif
    return mask;
}

// Counts console traffic on the retro port: joybus polls on the data pin while SInput waits for a
// USB host, and the SNES/NES latch then and while answering as an N64 (which owns the data pin).
// Stop it before another mode takes the pins.
static void _retro_port_watch(bool joybus, bool latch)
{
    const hoja_config_s *cfg = hoja_config_get();
    if(!cfg) return;

    joybus = joybus && AUTODETECT_GAMECUBE;
    latch = latch && AUTODETECT_SNES;

    const bool was_watching = _joybus_watching || _latch_watching;
    if(!was_watching && (joybus || latch))
    {
        gpio_add_raw_irq_handler_masked(_retro_port_mask(cfg), _retro_port_irq);
        irq_set_enabled(IO_IRQ_BANK0, true);
    }

    // Joybus idles high, the SNES latch low
#if AUTODETECT_GAMECUBE
    if(joybus != _joybus_watching)
    {
        const uint data_pin = cfg->joybus.data_pin;
        if(joybus)
        {
            _joybus_watching = true;
            gpio_init(data_pin);
            gpio_pull_up(data_pin);
            gpio_set_irq_enabled(data_pin, GPIO_IRQ_EDGE_FALL, true);
        }
        else
        {
            gpio_set_irq_enabled(data_pin, GPIO_IRQ_EDGE_FALL, false);
            _joybus_watching = false;
            gpio_disable_pulls(data_pin);
        }
    }
#endif

#if AUTODETECT_SNES
    if(latch != _latch_watching)
    {
        const uint latch_pin = cfg->nesbus.latch_pin;
        if(latch)
        {
            _latch_watching = true;
            gpio_init(latch_pin);
            gpio_pull_down(latch_pin);
            gpio_set_irq_enabled(latch_pin, GPIO_IRQ_EDGE_RISE, true);
        }
        else
        {
            gpio_set_irq_enabled(latch_pin, GPIO_IRQ_EDGE_RISE, false);
            _latch_watching = false;
            gpio_disable_pulls(latch_pin);
        }
    }
#endif

    if(was_watching && !joybus && !latch)
        gpio_remove_raw_irq_handler_masked(_retro_port_mask(cfg), _retro_port_irq);
}
#else
static void _retro_port_watch(bool joybus, bool latch)
{
    (void)joybus;
    (void)latch;
}
#endif

// A powered NES or SNES holds the latch low and the clock high, even before a game reads the
// controller. With nothing attached, or an N64 or GameCube, both read high from the pull-ups; a
// console that's switched off pulls both low.
#define AUTODETECT_NESBUS_PRESENT_MS 50

static bool _nesbus_console_present(uint64_t now_us)
{
#if AUTODETECT_SNES
    static uint64_t since_us = 0;
    const hoja_config_s *cfg = hoja_config_get();

    if(gpio_get(cfg->nesbus.latch_pin) || !gpio_get(cfg->nesbus.clock_pin))
    {
        since_us = 0;
        return false;
    }

    if(!since_us) since_us = now_us;
    return (now_us - since_us) >= (AUTODETECT_NESBUS_PRESENT_MS * 1000ull);
#else
    (void)now_us;
    return false;
#endif
}

static autodetect_power_t _power = AUTODETECT_POWER_EXTERNAL;
static core_reportformat_t _wireless = CORE_REPORTFORMAT_UNDEFINED;

// Where the dongle search falls back to: Bluetooth in this mode, then this phase
static core_reportformat_t _fallback_format = CORE_REPORTFORMAT_SWPRO;
static phase_t _fallback_phase = PHASE_BT;

// Auto's wireless order (the boot order in the gamepad settings): the dongle and the saved hosts, first
// to last, those switched off left out. Auto works through it: each host is called in its own mode
// (any never paired skipped), and the dongle is taken at its place if one was heard.
static uint8_t _order[AUTO_HOST_COUNT];
static uint8_t _order_len = 0;
static uint8_t _order_next = 0;   // The next entry to try
static uint8_t _order_at = 0xFF;  // The entry being tried (0xFF before the first)
static bool _walking = false;     // Auto on battery works through the order
static bool _dongle_heard = false; // A free dongle answered the listen, to take at its place

static void _order_load(void)
{
    static const uint8_t defaults[AUTO_HOST_COUNT] = {AUTO_HOST_DONGLE, AUTO_HOST_SWITCH, AUTO_HOST_WII, AUTO_HOST_STEAM};
    const uint8_t *order = gamepad_config->auto_wireless_order;

    // Each entry once, else the defaults
    uint8_t seen = 0;
    for(uint8_t i = 0; i < AUTO_HOST_COUNT; i++)
        seen |= (uint8_t)(1u << ((order[i] & ~AUTO_HOST_OFF) & 0x07));
    if((gamepad_config->auto_order_version != AUTO_ORDER_VERSION) || (seen != ((1u << AUTO_HOST_COUNT) - 1)))
        order = defaults;

    _order_len = 0;
    for(uint8_t i = 0; i < AUTO_HOST_COUNT; i++)
    {
        if(!(order[i] & AUTO_HOST_OFF))
            _order[_order_len++] = order[i];
    }
    _order_next = 0;
    _order_at = 0xFF;
    _dongle_heard = false;
}

static bool _order_has(uint8_t entry)
{
    for(uint8_t i = 0; i < _order_len; i++)
    {
        if(_order[i] == entry)
            return true;
    }
    return false;
}

static core_reportformat_t _host_format(uint8_t entry)
{
    switch(entry)
    {
        case AUTO_HOST_SWITCH: return CORE_REPORTFORMAT_SWPRO;
        case AUTO_HOST_WII:    return CORE_REPORTFORMAT_WII;
        case AUTO_HOST_STEAM:  return CORE_REPORTFORMAT_SINPUT;
        default:               return CORE_REPORTFORMAT_UNDEFINED;
    }
}

typedef enum
{
    NEXT_NONE,
    NEXT_HOST,   // Call this saved host
    NEXT_DONGLE, // Take the dongle that was heard
} next_t;

// The next entry worth trying: a saved host, or the dongle once one was heard. hosts_only: before the
// listen (the dongle's turn can't be known yet, it is taken when heard if it ranks first).
static next_t _order_step(core_reportformat_t *format, bool hosts_only)
{
    while(_order_next < _order_len)
    {
        const uint8_t i = _order_next++;
        if(_order[i] == AUTO_HOST_DONGLE)
        {
            if(!hosts_only && _dongle_heard)
            {
                _order_at = i;
                return NEXT_DONGLE;
            }
            continue;
        }
        const core_reportformat_t f = _host_format(_order[i]);
        if((f != CORE_REPORTFORMAT_UNDEFINED) && transport_bt_host_paired(f))
        {
            _order_at = i;
            *format = f;
            return NEXT_HOST;
        }
    }
    return NEXT_NONE;
}

// The dongle ranks above the entry being tried (or nothing is being tried yet)
static bool _dongle_ranks_first(void)
{
    for(uint8_t i = 0; i < _order_len; i++)
    {
        if(_order[i] == AUTO_HOST_DONGLE)
            return (_order_at == 0xFF) || (i < _order_at);
    }
    return false;
}


// Wireless in a mode the dongle can also carry: listen for a free dongle first (autodetect_dongle_listen)
static bool _listen_open = false;

static bool _dongle_carries(core_reportformat_t format)
{
    return AUTODETECT_DONGLE && ((format == CORE_REPORTFORMAT_SWPRO) || (format == CORE_REPORTFORMAT_SINPUT));
}

// Wireless in this mode over Bluetooth, after a listen for a free dongle. Auto (undefined) is Switch
// mode, and checks the saved hosts first.
static void _wireless_plan(core_reportformat_t mode, core_reportformat_t *format, gamepad_transport_t *transport,
                           phase_t *phase)
{
    *format = (mode == CORE_REPORTFORMAT_UNDEFINED) ? CORE_REPORTFORMAT_SWPRO : mode;
    _fallback_format = *format;
    _fallback_phase = (mode == CORE_REPORTFORMAT_UNDEFINED) ? PHASE_BT : PHASE_DONE;

    _walking = false;
    _listen_open = _dongle_carries(*format);

#if HOJA_BT_USES_FLOSS
    // Auto works through its boot order: each saved host called straight away in its own mode, the
    // first that answers connected; the dongle at its place when the listen heard one. With nothing
    // left, SInput waits to be found.
    if(mode == CORE_REPORTFORMAT_UNDEFINED)
    {
        _order_load();
        _walking = true;
        _listen_open = AUTODETECT_DONGLE && _order_has(AUTO_HOST_DONGLE);
        core_reportformat_t host;
        if(_order_step(&host, true) == NEXT_HOST)
        {
            _fallback_format = host;
            _fallback_phase = PHASE_BT_HOST;
        }
        else
        {
            _fallback_format = CORE_REPORTFORMAT_SINPUT;
            _fallback_phase = PHASE_DONE;
        }
    }
#endif

    *format = _fallback_format;
    *transport = GAMEPAD_TRANSPORT_BLUETOOTH;
    *phase = _fallback_phase;
}

static void _wireless_start(boot_info_s *info)
{
    phase_t phase;
    _wireless_plan(_wireless, &info->reportformat, &info->transport, &phase);
    _phase = phase;
    BOOT_TIMELINE(TL_PHASE, _phase);
    if(phase == PHASE_DONE)
        _confirmed = true;
}

static void _switch_to_wireless(void)
{
    core_reportformat_t format;
    gamepad_transport_t transport;
    phase_t phase;
    _wireless_plan(_wireless, &format, &transport, &phase);
    _switch_to(format, transport, phase);
}

void autodetect_wireless(boot_info_s *info, core_reportformat_t format)
{
    _power = AUTODETECT_POWER_BATTERY;
    _wireless = format;
    _wireless_start(info);
}

void autodetect_pairing(boot_info_s *info)
{
    _power = AUTODETECT_POWER_BATTERY;
    _wireless = info->reportformat;
    _fallback_format = info->reportformat;
    _fallback_phase = PHASE_DONE;
    _phase = PHASE_DONE;
    _confirmed = true;
    _listen_open = _dongle_carries(info->reportformat);
}

void autodetect_boot(boot_info_s *info, autodetect_power_t power, bool wired_auto, core_reportformat_t wireless)
{
    _power = power;
    _wireless = wireless;

    // Wired default picked by hand, wireless Auto, on battery: only the wireless search runs
    if(!wired_auto && AUTODETECT_BLUETOOTH)
    {
        _wireless_start(info);
        return;
    }

    // On battery, or on a board that can't tell what powers it, start as an N64: it has to be
    // answered from its first poll
    if((power != AUTODETECT_POWER_EXTERNAL) && AUTODETECT_N64)
    {
        // Set up exactly like a manual N64 boot, so an N64 is answered just as quickly
        info->reportformat = CORE_REPORTFORMAT_N64;
        info->transport = GAMEPAD_TRANSPORT_JOYBUS64;
        _phase = PHASE_N64;
        BOOT_TIMELINE(TL_PHASE, _phase);
    }
    else if((power == AUTODETECT_POWER_BATTERY) && AUTODETECT_BLUETOOTH)
    {
        // Nothing wired to answer on battery power
        _wireless_start(info);
    }
    else
    {
        info->reportformat = _usb_start_mode();
        info->transport = GAMEPAD_TRANSPORT_USB;
        _phase = PHASE_WIRED;
        BOOT_TIMELINE(TL_PHASE, _phase);
    }
}

bool autodetect_active(void)
{
    return _phase != PHASE_OFF;
}

bool autodetect_pending(void)
{
    return _phase != PHASE_OFF && !_confirmed;
}

bool autodetect_take_switch(core_reportformat_t *format, gamepad_transport_t *transport)
{
    if(!_switch_pending) return false;
    _switch_pending = false;
    *format = _switch_format;
    *transport = _switch_transport;
    return true;
}

bool autodetect_bt_probe_pending(void)
{
    return _phase == PHASE_BT && !_confirmed;
}

bool autodetect_bt_probe_result(core_reportformat_t found)
{
    if(_phase != PHASE_BT || _confirmed) return true;

    if(found == CORE_REPORTFORMAT_WII || found == CORE_REPORTFORMAT_SINPUT)
    {
        _switch_to(found, GAMEPAD_TRANSPORT_BLUETOOTH, PHASE_DONE);
        return false;
    }

    // No console awake: SInput, which reconnects to a saved PC or waits to be found
    if (found == CORE_REPORTFORMAT_UNDEFINED)
    {
        _switch_to(CORE_REPORTFORMAT_SINPUT, GAMEPAD_TRANSPORT_BLUETOOTH, PHASE_DONE);
        return false;
    }

    // Switch mode answered: carry on in it
    _confirmed = true;
    _phase = PHASE_DONE;
    return true;
}

// On to the next entry of the boot order: a saved host, the dongle that was heard, else SInput, which
// keeps calling a saved PC or waits to be found
static void _order_walk(void)
{
    core_reportformat_t host;
    switch(_order_step(&host, false))
    {
        case NEXT_HOST:
            _switch_to(host, GAMEPAD_TRANSPORT_BLUETOOTH, PHASE_BT_HOST);
            break;

        case NEXT_DONGLE:
            _dongle_heard = false;
            _confirmed = false;
            _switch_to(CORE_REPORTFORMAT_SWPRO, GAMEPAD_TRANSPORT_DONGLE, PHASE_DONGLE);
            break;

        default:
            _switch_to(CORE_REPORTFORMAT_SINPUT, GAMEPAD_TRANSPORT_BLUETOOTH, PHASE_DONE);
            break;
    }
}

bool autodetect_bt_calling(void)
{
    return _phase == PHASE_BT_HOST && !_confirmed && !_switch_pending;
}

void autodetect_bt_call_result(bool connected)
{
    if(!autodetect_bt_calling()) return;

    if(connected)
    {
        _confirmed = true;
        _phase = PHASE_DONE;
        return;
    }

    _order_walk();
}

bool autodetect_dongle_listen(void)
{
    return _listen_open && !_switch_pending;
}

void autodetect_dongle_listen_end(void)
{
    _listen_open = false;
}

bool autodetect_dongle_heard(void)
{
    if(!autodetect_dongle_listen()) return false;
    _listen_open = false;

    // Auto's boot order with a host above the dongle: that host is called first, the dongle kept for
    // its place
    if(_walking && !_dongle_ranks_first())
    {
        _dongle_heard = true;
        return false;
    }

    // Until it connects; a dongle that doesn't goes back to this start's Bluetooth plan
    _confirmed = false;
    const core_reportformat_t format = (_wireless == CORE_REPORTFORMAT_UNDEFINED) ? CORE_REPORTFORMAT_SWPRO : _wireless;
    _switch_to(format, GAMEPAD_TRANSPORT_DONGLE, PHASE_DONGLE);
    return true;
}

void autodetect_dongle_found(void)
{
    if(_phase != PHASE_DONGLE || _confirmed) return;
    _confirmed = true;
    _phase = PHASE_DONE;
}

void autodetect_switch_done(void)
{
    // A switch to the detected mode confirms it once that mode is up (its colors are loaded)
    if(_phase == PHASE_DONE)
        _confirmed = true;
}

autodetect_power_t autodetect_power(void)
{
    return _power;
}

bool autodetect_mode_open(void)
{
    return (_phase != PHASE_OFF) && !_confirmed && (_wireless == CORE_REPORTFORMAT_UNDEFINED);
}

void autodetect_pause(bool paused)
{
    _paused = paused;
}

bool autodetect_settle_for_app(void)
{
    if (!autodetect_mode_open() || (_phase == PHASE_DONE) || _switch_pending)
        return false;
    if (_power == AUTODETECT_POWER_EXTERNAL)
        return false;

    _retro_port_watch(false, false);
    _switch_to(CORE_REPORTFORMAT_SINPUT, GAMEPAD_TRANSPORT_BLUETOOTH, PHASE_DONE);
    return true;
}

void autodetect_on_connected(void)
{
    if(_phase == PHASE_N64) _confirmed = true;
}

void autodetect_on_usb_host(void)
{
    _usb_host = true;
}

void autodetect_on_usb_bos(void)
{
    _usb_bos = true;
}

void autodetect_task(uint64_t now_us)
{
    static uint64_t usb_host_us = 0;

    // An N64 confirms itself by polling. It needs no radio.
    if(_confirmed && _phase == PHASE_N64)
    {
        _retro_port_watch(false, false);
        transport_bt_radio_off();
    }

    if(_confirmed || _switch_pending) return;
    if(_phase == PHASE_OFF || _phase == PHASE_DONE) return;

    if(!_phase_started)
    {
        _phase_started = true;
        _phase_start_us = now_us;
        if(_phase == PHASE_WIRED) _retro_port_watch(true, true);
        if(_phase == PHASE_N64)
        {
#if AUTODETECT_N64 && AUTODETECT_GAMECUBE
            transport_jb64_command_counts(&_n64_probes_start, &_n64_polls_start);
#endif
            _retro_port_watch(false, true);
            // On battery the radio starts while we wait for polls, rather than after. Polls are
            // answered meanwhile; the next pass sees any that came, before the wait is judged.
            if((_power == AUTODETECT_POWER_BATTERY) && AUTODETECT_BLUETOOTH)
            {
                transport_bt_radio_warm();
                return;
            }
        }
    }
    if(_paused)
    {
        _phase_start_us = now_us;
        return;
    }
    const uint32_t elapsed_ms = (uint32_t)((now_us - _phase_start_us) / 1000);

    switch(_phase)
    {
        case PHASE_N64:
        // An NES or SNES running on its own power (no external power seen at boot)
        if(AUTODETECT_SNES && (_nesbus_console_present(now_us) || _latch_edges >= AUTODETECT_LATCH_EDGES))
        {
            _retro_port_watch(false, false);
            transport_bt_radio_off();
            _switch_to(CORE_REPORTFORMAT_SNES, GAMEPAD_TRANSPORT_NESBUS, PHASE_DONE);
            break;
        }
#if AUTODETECT_N64 && AUTODETECT_GAMECUBE
        // A GameCube, told by its traffic alone: on battery, or with its 5V off, as well
        {
            uint32_t probes, polls;
            transport_jb64_command_counts(&probes, &polls);
            if(((probes - _n64_probes_start) >= AUTODETECT_GAMECUBE_PROBES) && (polls == _n64_polls_start))
            {
                _retro_port_watch(false, false);
                transport_bt_radio_off();
                _switch_to(CORE_REPORTFORMAT_GAMECUBE, GAMEPAD_TRANSPORT_JOYBUSGC, PHASE_DONE);
                break;
            }
        }
#endif
        if(elapsed_ms < AUTODETECT_N64_WAIT_MS)
            break;
        _retro_port_watch(false, false);
        // On battery look for a wireless host, otherwise (or without a radio) carry on wired
        if((_power == AUTODETECT_POWER_BATTERY) && AUTODETECT_BLUETOOTH)
            _switch_to_wireless();
        else
            _switch_to(_usb_start_mode(), GAMEPAD_TRANSPORT_USB, PHASE_WIRED);
        break;

        case PHASE_WIRED:
        if(_usb_host && !usb_host_us) usb_host_us = now_us;

        // Always Switch: any USB host is answered as a Switch
        if(usb_host_us && (_usb_start_mode() == CORE_REPORTFORMAT_SWPRO))
        {
            _retro_port_watch(false, false);
            _confirmed = true;
            _phase = PHASE_DONE;
        }
        // Computers ask for the BOS descriptor while connecting, a Switch never does (and may
        // never finish setting up a controller it does not know). A computer gets the boot order's
        // mode for it.
        else if(_usb_bos)
        {
            _retro_port_watch(false, false);
            const core_reportformat_t computer = _usb_computer_mode();
            if(computer == CORE_REPORTFORMAT_SINPUT)
            {
                _confirmed = true;
                _phase = PHASE_DONE;
            }
            else
                _switch_to(computer, GAMEPAD_TRANSPORT_USB, PHASE_DONE);
        }
        else if(usb_host_us && (now_us - usb_host_us) >= (AUTODETECT_BOS_WAIT_MS * 1000ull))
        {
            _retro_port_watch(false, false);
            _switch_to(CORE_REPORTFORMAT_SWPRO, GAMEPAD_TRANSPORT_USB, PHASE_DONE);
        }
        else if(AUTODETECT_GAMECUBE && _joybus_edges >= AUTODETECT_JOYBUS_EDGES)
        {
            _retro_port_watch(false, false);
            _switch_to(CORE_REPORTFORMAT_GAMECUBE, GAMEPAD_TRANSPORT_JOYBUSGC, PHASE_DONE);
        }
        else if(AUTODETECT_SNES && (_nesbus_console_present(now_us) || _latch_edges >= AUTODETECT_LATCH_EDGES))
        {
            _retro_port_watch(false, false);
            _switch_to(CORE_REPORTFORMAT_SNES, GAMEPAD_TRANSPORT_NESBUS, PHASE_DONE);
        }
        else if(AUTODETECT_BLUETOOTH && !usb_host_us && elapsed_ms >= AUTODETECT_WIRED_WAIT_MS)
        {
            _retro_port_watch(false, false);
            _switch_to_wireless();
        }
        break;

        case PHASE_DONGLE:
        if(elapsed_ms >= AUTODETECT_DONGLE_WAIT_MS)
        {
            // Auto carries on down its boot order; a chosen mode goes to Bluetooth in that mode
            if(_walking)
                _order_walk();
            else
                _switch_to(_fallback_format, GAMEPAD_TRANSPORT_BLUETOOTH, _fallback_phase);
        }
        break;

        case PHASE_BT_HOST:
        case PHASE_BT:
        // Should the radio never come up, carry on in this mode
        if(elapsed_ms >= AUTODETECT_BT_PROBE_MAX_MS)
        {
            _confirmed = true;
            _phase = PHASE_DONE;
        }
        break;

        default:
        break;
    }
}
