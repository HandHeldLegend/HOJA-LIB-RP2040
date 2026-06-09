#include "board_config.h"

#if defined(HOJA_TRANSPORT_WLAN_DRIVER) && (HOJA_TRANSPORT_WLAN_DRIVER == WLAN_DRIVER_HAL)

#include <string.h>
#include <stdio.h>

#include "hoja.h"

#include "cores/cores.h"
#include "transport/transport.h"
#include "transport/transport_wlan.h"

#include "utilities/settings.h"

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

static struct udp_pcb *_wlan_pcb = NULL;
static core_params_s *_wlan_core_params = NULL;
static bool _wlan_running = false;

static dongle_cfg_gamepad_s _wlan_dgp_cfg = {0};
static dongle_pkt_s _wlan_rx_pkt = {0};

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

#if !defined(HOJA_DEVICE_MAKER)
#define WLAN_DGP_MAKER "HHL"
#else
#define WLAN_DGP_MAKER HOJA_DEVICE_MAKER
#endif

static void _wlan_fill_dgp_cfg(core_params_s *params)
{
    memset(&_wlan_dgp_cfg, 0, sizeof(_wlan_dgp_cfg));

    _wlan_dgp_cfg.mode = _wlan_mode_from_format(params->core_report_format);
    _wlan_dgp_cfg.evt.rumble = true;
    _wlan_dgp_cfg.evt.player_number = true;
    _wlan_dgp_cfg.evt.transport_status = true;

    dongle_wake_strcopy(_wlan_dgp_cfg.manufacturer, DONGLE_WAKE_MANUFACTURER_LEN, WLAN_DGP_MAKER);

    if (params->hid_device != NULL)
    {
        _wlan_dgp_cfg.vid = params->hid_device->vid;
        _wlan_dgp_cfg.pid = params->hid_device->pid;
        dongle_wake_strcopy(_wlan_dgp_cfg.name, DONGLE_WAKE_NAME_LEN, params->hid_device->name);
    }
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

    dongle_api_gamepad_udp_rx(&_wlan_rx_pkt);
}

// --- dongle_api_gamepad_hook_* ---

dongle_link_status_t dongle_api_gamepad_hook_link_up(void)
{
    int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    return (link == CYW43_LINK_UP) ? DONGLE_LINK_UP : DONGLE_LINK_DOWN;
}

bool dongle_api_gamepad_hook_apply_static_ip(uint8_t addr[4], uint8_t mask[4], uint8_t gateway[4])
{
    ip4_addr_t target;
    ip4_addr_t m;
    ip4_addr_t gw;

    IP4_ADDR(&target, addr[0], addr[1], addr[2], addr[3]);
    IP4_ADDR(&m, mask[0], mask[1], mask[2], mask[3]);
    IP4_ADDR(&gw, gateway[0], gateway[1], gateway[2], gateway[3]);

    struct netif *nif = netif_default;
    if (nif == NULL)
        return false;

    /*
     * dhcp_stop() sends RELEASE and clears the netif; always re-apply the
     * gamepad address afterward so our source IP matches the dongle RX filter
     * (192.168.4.16), even when DHCP already offered the correct lease.
     */
    dhcp_stop(nif);
    netif_set_addr(nif, &target, &m, &gw);

    cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);

    return true;
}

void dongle_api_gamepad_hook_connect_async(const char *ssid, const char *pw)
{
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_set_roam_enabled(&cyw43_state, false);
    cyw43_wifi_set_interference_mode(&cyw43_state, CYW43_IFMODE_NONE);

    const char *join_ssid = ssid;

    if (gamepad_config->wlan_dongle_key != 0u)
    {
        static char local_ssid[32];
        snprintf(local_ssid, sizeof(local_ssid), "HOJA_WLAN_%04u",
                 (unsigned)gamepad_config->wlan_dongle_key);
        join_ssid = local_ssid;
    }

    cyw43_arch_wifi_connect_async(join_ssid, pw, CYW43_AUTH_WPA2_AES_PSK);
}

void dongle_api_gamepad_hook_udp_tx(const dongle_pkt_s *pkt, uint8_t ip[4], uint16_t port)
{
    if (_wlan_pcb == NULL || pkt == NULL)
        return;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(dongle_pkt_s), PBUF_RAM);
    if (p == NULL)
        return;

    memcpy(p->payload, pkt, sizeof(dongle_pkt_s));

    ip_addr_t dst;
    IP4_ADDR(ip_2_ip4(&dst), ip[0], ip[1], ip[2], ip[3]);
    IP_SET_TYPE(&dst, IPADDR_TYPE_V4);

    udp_sendto(_wlan_pcb, p, &dst, port);
    pbuf_free(p);
}

bool dongle_api_gamepad_hook_get_inputreport(uint8_t data[64], uint16_t *len, bool *reliable)
{
    core_report_s report = {0};
    if (!core_get_generated_report(&report))
        return false;

    memset(data, 0, 64);
    memcpy(data, report.data, 64);

    if (len != NULL)
        *len = 64;

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
    _wlan_running = false;
    _wlan_notify_disconnected();
    _wlan_stack_teardown();
    _wlan_core_params = NULL;
}

bool transport_wlan_init(core_params_s *params)
{
    _wlan_core_params = params;
    _wlan_fill_dgp_cfg(params);

    if (!_wlan_radio_init())
        return false;

    while (!_wlan_udp_bind())
        sys_hal_sleep_ms(1000);

    dongle_api_gamepad_wlan_init(&_wlan_dgp_cfg);

    _wlan_running = true;
    return true;
}

void transport_wlan_task(uint64_t timestamp)
{
    (void)timestamp;

    if (!_wlan_running)
        return;

    dongle_api_gamepad_wlan_task();

    if (_wlan_core_params != NULL && _wlan_core_params->sys_gyro_task != NULL &&
        dongle_api_gamepad_hook_link_up() == DONGLE_LINK_UP)
    {
        static uint64_t last_gyro_us = 0;
        uint64_t now = time_us_64();

        if ((now - last_gyro_us) >= 1000u)
        {
            last_gyro_us = now;
            _wlan_core_params->sys_gyro_task();
        }
    }
}

#endif
