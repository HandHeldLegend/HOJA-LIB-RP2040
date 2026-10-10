#include "board_config.h"

#if defined(HOJA_TRANSPORT_WLAN_DRIVER) && (HOJA_TRANSPORT_WLAN_DRIVER == WLAN_DRIVER_HAL)

#include <string.h>

#include "hoja.h"

#include "cores/cores.h"
#include "transport/transport.h"
#include "transport/transport_wlan.h"

#include "utilities/settings.h"
#include "utilities/static_config.h"
#include "utilities/boot.h"
#include "usb/webusb.h"

#include "pico/cyw43_arch.h"
#include "pico/rand.h"
#include "pico/time.h"
#include "hal/sys_hal.h"

#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "dongle.h"
#include "dongle_gamepad.h"
#include "transport/transport_wlan.h"

/*
 * HOJA WLAN dongle transport — platform adapter for HOJA-LIB-DONGLE gamepad SM.
 *
 * Protocol pacing, session/WAKE handling, STATUS dispatch, and reliable-lane
 * dedup live in dongle_gamepad.c. This file implements the cyw43/lwIP hooks
 * and the transport_wlan_* entry points used by the rest of the firmware.
 *
 * Adapted from the working Pico-W-NS-Example ns_wlan.c reference.
 */

#define NS_WLAN_SWITCH_FULL_REPORT_ID 0x30

#define WLAN_RX_IDLE_SHUTDOWN_US (4ULL * 1000ULL * 1000ULL)

static struct udp_pcb *_wlan_pcb = NULL;
static core_params_s *_wlan_core_params = NULL;
static bool _wlan_running = false;
static volatile bool _wlan_rx_pending = false;
static uint64_t _wlan_last_rx_us = 0;
static bool _wlan_rx_seen = false;
static bool _wlan_idle_shutdown_sent = false;

static dongle_cfg_gamepad_s _wlan_dgp_cfg = {0};
static dongle_pkt_s _wlan_rx_pkt = {0};

// --- Config app through the dongle ---

// Commands from the app arrive in the network callback; they run from the task loop (some write
// flash)
#define WLAN_CONFIG_RX_LEN 8
typedef struct
{
    uint16_t len;
    uint8_t  data[64];
} wlan_config_rx_s;

static wlan_config_rx_s _wlan_config_rx[WLAN_CONFIG_RX_LEN];
static volatile uint8_t _wlan_config_rx_head = 0;
static volatile uint8_t _wlan_config_rx_tail = 0;

void dongle_api_gamepad_hook_config_rx(const uint8_t data[64], uint16_t len)
{
    uint8_t next = (uint8_t)((_wlan_config_rx_head + 1u) % WLAN_CONFIG_RX_LEN);
    if (next == _wlan_config_rx_tail || len == 0 || len > 64)
        return;

    _wlan_config_rx[_wlan_config_rx_head].len = len;
    memcpy(_wlan_config_rx[_wlan_config_rx_head].data, data, len);
    _wlan_config_rx_head = next;
}

static void _wlan_config_task(uint64_t timestamp)
{
    while (_wlan_config_rx_tail != _wlan_config_rx_head)
    {
        wlan_config_rx_s *rx = &_wlan_config_rx[_wlan_config_rx_tail];
        webusb_command_handler(rx->data, rx->len);
        _wlan_config_rx_tail = (uint8_t)((_wlan_config_rx_tail + 1u) % WLAN_CONFIG_RX_LEN);
    }

    webusb_send_rawinput(timestamp);
}

// Replies queue in the dongle library; wait a little if a big block fills it
#define WLAN_CONFIG_SEND_WAIT_US 200000

static bool _wlan_sink_send(const uint8_t *data, uint16_t size)
{
    uint64_t until = time_us_64() + WLAN_CONFIG_SEND_WAIT_US;
    for (;;)
    {
        cyw43_arch_lwip_begin();
        bool queued = dongle_api_gamepad_config_send(data, size);
        cyw43_arch_lwip_end();

        if (queued)
            return true;
        if (time_us_64() > until)
            return false;
        sys_hal_sleep_ms(1);
    }
}

static bool _wlan_sink_send_input(const uint8_t *data, uint16_t size)
{
    cyw43_arch_lwip_begin();
    dongle_api_gamepad_bulk_send(data, size);
    cyw43_arch_lwip_end();
    return true;
}

static bool _wlan_sink_ready(int timeout_ms)
{
    (void)timeout_ms;
    return dongle_api_gamepad_adopted();
}

static const webusb_sink_s _wlan_webusb_sink = {
    .send       = _wlan_sink_send,
    .send_input = _wlan_sink_send_input,
    .ready      = _wlan_sink_ready,
};

// A newly paired dongle, saved from the task loop (it is found in the network callback)
static volatile bool _wlan_paired_pending = false;
static volatile uint16_t _wlan_paired_key = 0;

static void _wlan_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                           const ip_addr_t *addr, u16_t port);
static bool _wlan_udp_bind(void);

// --- dongle utility hooks ---

uint64_t dongle_api_hook_get_time_us_u64(void)
{
    return time_us_64();
}

uint16_t dongle_api_hook_get_rand_u16(void)
{
    return (uint16_t)(get_rand_32() & 0xFFFFu);
}

// --- Mode / config helpers ---

static dongle_mode_t _wlan_mode_from_format(core_reportformat_t fmt)
{
    switch (fmt)
    {
        case CORE_REPORTFORMAT_SWPRO:    return DONGLE_MODE_SWITCH;
        case CORE_REPORTFORMAT_SINPUT:   return DONGLE_MODE_SINPUT;
        case CORE_REPORTFORMAT_XINPUT:   return DONGLE_MODE_XINPUT;
        case CORE_REPORTFORMAT_SLIPPI:   return DONGLE_MODE_SLIPPI;
        case CORE_REPORTFORMAT_SNES:     return DONGLE_MODE_SNES;
        case CORE_REPORTFORMAT_N64:      return DONGLE_MODE_N64;
        case CORE_REPORTFORMAT_GAMECUBE: return DONGLE_MODE_GAMECUBE;
        default:                         return DONGLE_MODE_SWITCH;
    }
}

// Modes the dongle can present, for following it
static core_reportformat_t _wlan_format_from_mode(dongle_mode_t mode)
{
    switch (mode)
    {
        case DONGLE_MODE_SWITCH:   return CORE_REPORTFORMAT_SWPRO;
        case DONGLE_MODE_SINPUT:   return CORE_REPORTFORMAT_SINPUT;
        case DONGLE_MODE_XINPUT:   return CORE_REPORTFORMAT_XINPUT;
        case DONGLE_MODE_SLIPPI:   return CORE_REPORTFORMAT_SLIPPI;
        case DONGLE_MODE_N64:      return CORE_REPORTFORMAT_N64;
        case DONGLE_MODE_GAMECUBE: return CORE_REPORTFORMAT_GAMECUBE;
        default:                   return CORE_REPORTFORMAT_UNDEFINED;
    }
}

// The dongle's mode, picked up by the task loop (it arrives in the network callback)
static volatile core_reportformat_t _wlan_mode_request = CORE_REPORTFORMAT_UNDEFINED;

static void _wlan_fill_dgp_cfg(core_params_s *params)
{
    const hoja_config_s *cfg = hoja_config_get();
    const char *dev_maker = (cfg && cfg->device_maker) ? cfg->device_maker : "HHL";
    const char *dev_name  = (cfg && cfg->device_name)  ? cfg->device_name  : NULL;

    memset(&_wlan_dgp_cfg, 0, sizeof(_wlan_dgp_cfg));

    _wlan_dgp_cfg.mode = _wlan_mode_from_format(params->core_report_format);
    const boot_info_s *boot = boot_get_info();
    _wlan_dgp_cfg.mode_forced = boot && boot->mode_chosen;
    _wlan_dgp_cfg.pairing = (params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0;
    _wlan_dgp_cfg.fw_version = FIRMWARE_VERSION_TIMESTAMP;
    _wlan_dgp_cfg.evt.rumble = true;
    _wlan_dgp_cfg.evt.player_number = true;
    _wlan_dgp_cfg.evt.transport_status = true;

    dongle_wake_strcopy(_wlan_dgp_cfg.manufacturer, DONGLE_WAKE_MANUFACTURER_LEN, dev_maker);

    if (params->hid_device != NULL)
    {
        _wlan_dgp_cfg.vid = params->hid_device->vid;
        _wlan_dgp_cfg.pid = params->hid_device->pid;
        dongle_wake_strcopy(_wlan_dgp_cfg.name, DONGLE_WAKE_NAME_LEN, params->hid_device->name);
    }
    else if (cfg && (cfg->usb_vid || cfg->usb_pid))
    {
        _wlan_dgp_cfg.vid = cfg->usb_vid;
        _wlan_dgp_cfg.pid = cfg->usb_pid;
        if (dev_name)
            dongle_wake_strcopy(_wlan_dgp_cfg.name, DONGLE_WAKE_NAME_LEN, dev_name);
    }

    dongle_wlan_pin_from_u16((uint16_t)(gamepad_config->wlan_dongle_key % 10000u),
                             _wlan_dgp_cfg.pin);
}

static bool _wlan_radio_init(void)
{
    while (cyw43_arch_init())
    {
        sys_hal_sleep_ms(1000);
    }

    return true;
}

static bool _wlan_udp_bind(void)
{
    if (_wlan_pcb != NULL)
        return true;

    _wlan_pcb = udp_new();
    if (_wlan_pcb == NULL)
        return false;

    if (udp_bind(_wlan_pcb, IP_ANY_TYPE, DONGLE_WLAN_PORT) != ERR_OK)
    {
        udp_remove(_wlan_pcb);
        _wlan_pcb = NULL;
        return false;
    }

    udp_recv(_wlan_pcb, _wlan_udp_recv, NULL);
    return true;
}

static void _wlan_stack_teardown(void)
{
    if (_wlan_pcb != NULL)
    {
        udp_remove(_wlan_pcb);
        _wlan_pcb = NULL;
    }

    cyw43_arch_deinit();
}

static void _wlan_notify_disconnected(void)
{
    tp_evt_s evt = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {.connection = TP_CONNECTION_DISCONNECTED},
    };
    transport_evt_cb(evt);
}

// --- RX path ---

static void _wlan_udp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                           const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    (void)pcb;
    (void)addr;
    (void)port;

    if (p == NULL)
        return;

    if (p->tot_len != sizeof(dongle_pkt_s))
    {
        pbuf_free(p);
        return;
    }

    pbuf_copy_partial(p, &_wlan_rx_pkt, sizeof(dongle_pkt_s), 0);
    pbuf_free(p);

    _wlan_rx_pending = true;

    dongle_api_gamepad_udp_rx(&_wlan_rx_pkt);
}

// Only a dongle that adopted us counts: beacons from other dongles, and the gaps while trying
// the next one, must not run the idle shutdown
static void _wlan_service_rx_activity(uint64_t timestamp)
{
    if (!_wlan_rx_pending || !dongle_api_gamepad_adopted())
        return;

    _wlan_rx_pending = false;
    _wlan_last_rx_us = timestamp;
    _wlan_rx_seen = true;
}

static void _wlan_check_rx_idle_shutdown(uint64_t timestamp)
{
    if (!_wlan_rx_seen || _wlan_idle_shutdown_sent)
        return;

    if (_wlan_last_rx_us == 0)
        return;

    if (timestamp - _wlan_last_rx_us < WLAN_RX_IDLE_SHUTDOWN_US)
        return;

    _wlan_idle_shutdown_sent = true;

    tp_evt_s evt = {
        .evt = TP_EVT_POWERCOMMAND,
        .evt_powercommand = {.power_command = TP_POWERCOMMAND_SHUTDOWN},
    };
    transport_evt_cb(evt);
}

// --- dongle_api_gamepad_hook_* ---

// Power saving makes the radio sleep through packets, so it is turned off once per join
static bool _wlan_pm_set = false;

// Up once joined and given an address by the dongle's DHCP
dongle_link_status_t dongle_api_gamepad_hook_link_up(void)
{
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP)
        return DONGLE_LINK_DOWN;

    if (!_wlan_pm_set)
    {
        cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
        _wlan_pm_set = true;
    }

    return DONGLE_LINK_UP;
}

static int _wlan_scan_cb(void *env, const cyw43_ev_scan_result_t *result)
{
    (void)env;
    if (result != NULL)
        dongle_api_gamepad_scan_result((const char *)result->ssid, result->ssid_len, result->rssi);
    return 0;
}

static void _wlan_sta_setup(void)
{
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_set_roam_enabled(&cyw43_state, false);
    cyw43_wifi_set_interference_mode(&cyw43_state, CYW43_IFMODE_NONE);
}

bool dongle_api_gamepad_hook_scan_start(void)
{
    _wlan_sta_setup();

    cyw43_wifi_scan_options_t opts = {0};
    return cyw43_wifi_scan(&cyw43_state, &opts, NULL, _wlan_scan_cb) == 0;
}

bool dongle_api_gamepad_hook_scan_active(void)
{
    return cyw43_wifi_scan_active(&cyw43_state);
}

void dongle_api_gamepad_hook_connect_async(const char *ssid, const char *pw)
{
    _wlan_pm_set = false;
    _wlan_sta_setup();
    cyw43_arch_wifi_connect_async(ssid, pw, CYW43_AUTH_WPA2_AES_PSK);
}

void dongle_api_gamepad_hook_disconnect(void)
{
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
}

bool dongle_api_gamepad_hook_mode_request(dongle_mode_t mode)
{
    core_reportformat_t format = _wlan_format_from_mode(mode);
    if (format == CORE_REPORTFORMAT_UNDEFINED)
        return false;

    _wlan_mode_request = format;
    return true;
}

bool transport_wlan_take_mode(core_reportformat_t *format)
{
    if (!_wlan_running || _wlan_mode_request == CORE_REPORTFORMAT_UNDEFINED)
        return false;

    *format = _wlan_mode_request;
    _wlan_mode_request = CORE_REPORTFORMAT_UNDEFINED;
    return true;
}

bool transport_wlan_choosing(void)
{
    return _wlan_running && !_wlan_dgp_cfg.mode_forced && !dongle_api_gamepad_adopted();
}

void dongle_api_gamepad_hook_paired(const uint8_t pin[4])
{
    _wlan_paired_key = dongle_wlan_pin_to_u16(pin);
    _wlan_paired_pending = true;
}

static void _wlan_save_paired(void)
{
    if (!_wlan_paired_pending)
        return;

    _wlan_paired_pending = false;
    gamepad_config->wlan_dongle_key = _wlan_paired_key;
    settings_commit_blocks();
}

// Replies go out from the network callback, but the hello comes from the task loop, where lwIP
// must be locked against the background interrupt it runs in (the lock nests)
void dongle_api_gamepad_hook_udp_tx(const dongle_pkt_s *pkt, uint8_t ip[4], uint16_t port)
{
    if (_wlan_pcb == NULL || pkt == NULL)
        return;

    cyw43_arch_lwip_begin();

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(dongle_pkt_s), PBUF_RAM);
    if (p == NULL)
    {
        cyw43_arch_lwip_end();
        return;
    }

    memcpy(p->payload, pkt, sizeof(dongle_pkt_s));

    ip_addr_t dst;
    IP4_ADDR(ip_2_ip4(&dst), ip[0], ip[1], ip[2], ip[3]);
    IP_SET_TYPE(&dst, IPADDR_TYPE_V4);

    udp_sendto(_wlan_pcb, p, &dst, port);
    pbuf_free(p);

    cyw43_arch_lwip_end();
}

bool dongle_api_gamepad_hook_get_inputreport(uint8_t data[64], uint16_t *len, bool *reliable)
{
    core_report_s report = {0};
    if (!core_get_generated_report(&report))
        return false;

    memset(data, 0, 64);

    uint16_t n = report.size;
    if (n > 64)
        n = 64;

    if (n > 0)
        memcpy(data, report.data, n);

    if (len != NULL)
        *len = n;

    if (reliable != NULL)
        *reliable = report.reliable;

    return true;
}

void dongle_api_gamepad_hook_set_outputreport(const uint8_t data[64], uint16_t len)
{
    core_report_tunnel_cb(data, len);
}

void dongle_api_gamepad_hook_set_player(uint8_t player_number)
{
    tp_evt_s evt = {
        .evt = TP_EVT_PLAYERLED,
        .evt_playernumber = {.player_number = player_number},
    };
    transport_evt_cb(evt);
}

void dongle_api_gamepad_hook_set_transport(bool connected)
{
    tp_evt_s evt = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {
            .connection = connected ? TP_CONNECTION_CONNECTED : TP_CONNECTION_DISCONNECTED,
        },
    };
    transport_evt_cb(evt);
}

void dongle_api_gamepad_hook_set_rumble(uint8_t left, uint8_t right,
                                        uint8_t left_brake, uint8_t right_brake)
{
    tp_evt_s evt = {
        .evt = TP_EVT_ERMRUMBLE,
        .evt_ermrumble = {
            .left = left,
            .right = right,
            .leftbrake = left_brake,
            .rightbrake = right_brake,
        },
    };
    transport_evt_cb(evt);
}

void dongle_api_gamepad_hook_reset_network(void)
{
    if (_wlan_pcb != NULL)
    {
        udp_remove(_wlan_pcb);
        _wlan_pcb = NULL;
    }

    cyw43_arch_deinit();

    while (cyw43_arch_init())
        sys_hal_sleep_ms(1000);

    while (!_wlan_udp_bind())
        sys_hal_sleep_ms(1000);
}

// --- transport_wlan_* API ---

void transport_wlan_stop(void)
{
    webusb_set_sink(NULL);
    _wlan_running = false;
    _wlan_rx_pending = false;
    _wlan_last_rx_us = 0;
    _wlan_rx_seen = false;
    _wlan_idle_shutdown_sent = false;
    _wlan_notify_disconnected();
    _wlan_stack_teardown();
    _wlan_core_params = NULL;
}

bool transport_wlan_init(core_params_s *params)
{
    // Already up: the mode changed to follow the dongle, so only our identity changes
    if (_wlan_running)
    {
        _wlan_core_params = params;
        _wlan_fill_dgp_cfg(params);
        cyw43_arch_lwip_begin();
        dongle_api_gamepad_set_identity(_wlan_dgp_cfg.mode, _wlan_dgp_cfg.vid, _wlan_dgp_cfg.pid,
                                        _wlan_dgp_cfg.name, _wlan_dgp_cfg.manufacturer);
        cyw43_arch_lwip_end();
        return true;
    }

    _wlan_core_params = params;
    _wlan_fill_dgp_cfg(params);

    if (!_wlan_radio_init())
        return false;

    while (!_wlan_udp_bind())
        sys_hal_sleep_ms(1000);

    dongle_api_gamepad_wlan_init(&_wlan_dgp_cfg);

    _wlan_rx_pending = false;
    _wlan_last_rx_us = 0;
    _wlan_rx_seen = false;
    _wlan_idle_shutdown_sent = false;
    _wlan_config_rx_head = 0;
    _wlan_config_rx_tail = 0;
    webusb_set_sink(&_wlan_webusb_sink);
    _wlan_running = true;
    return true;
}

void transport_wlan_task(uint64_t timestamp)
{
    if (!_wlan_running)
        return;

    _wlan_service_rx_activity(timestamp);
    dongle_api_gamepad_wlan_task();
    _wlan_save_paired();
    _wlan_config_task(timestamp);
    _wlan_check_rx_idle_shutdown(timestamp);
}

static uint32_t _wlan_hal_probe_wireless(void)
{
    if (cyw43_arch_init())
    {
        return 0;
    }

    cyw43_arch_deinit();
    return 1;
}

uint8_t transport_wlan_static_supported(void)
{
    return 1;
}

uint8_t transport_wlan_static_part_status(void)
{
    return _wlan_hal_probe_wireless() > 0u ? TRANSPORT_WIRELESS_PART_OK : TRANSPORT_WIRELESS_PART_ERROR;
}

#endif
