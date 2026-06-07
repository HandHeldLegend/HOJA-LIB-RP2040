#include "board_config.h"

#if defined(HOJA_TRANSPORT_WLAN_DRIVER) && (HOJA_TRANSPORT_WLAN_DRIVER == WLAN_DRIVER_HAL)

#include <string.h>
#include <hoja.h>

#include "hal/wlan_hal.h"
#include "cores/cores.h"
#include "transport/transport.h"
#include "transport/transport_wlan.h"

#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "utilities/interval.h"
#include "utilities/settings.h"

#include "hal/sys_hal.h"

// Shared dongle protocol definitions (HOJA-LIB-DONGLE).
#include "dongle.h"

/*
 * HOJA WLAN dongle transport.
 *
 * Implements the gamepad side of the HOJA dongle UDP protocol described in
 * HOJA-LIB-DONGLE/docs/GAMEPAD_IMPLEMENTATION.md.
 *
 * Golden rule: the gamepad never transmits UDP proactively. Every dongle_pkt_s
 * we send is a *reply* to a packet the dongle just sent (WAKE beacon, STATUS, or
 * CORE_RELIABLE). All UDP TX therefore happens inside the lwIP receive callback
 * (lwIP/background context under pico_cyw43_arch_lwip_threadsafe_background, so
 * no extra locking is required there).
 *
 * The main task loop only drives the Wi-Fi association state machine, the 5 s
 * link watchdog, and IMU/gyro sampling (kept in main-loop context where blocking
 * peripheral access is safe).
 */

// --- Wi-Fi / endpoint configuration ---
#define WIFI_SSID_BASE "HOJA_WLAN_"
#define WIFI_SSID_VALUE 1234u
#define WIFI_PASS       "HOJA_1234"
#define AP_IP           "192.168.4.1"

static struct udp_pcb *pcb = NULL;
static ip_addr_t ap_addr;

static core_params_s *_wlan_core_params = NULL;

// Current logical session (mode + random 12-bit id). New id per cold boot / reset.
static dongle_session_s _session = {0};

// Last reliable-layer ack token requested by the dongle. Echoed on every reply.
static volatile uint16_t _ack_echo = 0;

// Snapshot of the last status received from the dongle (rumble/brake/conn/player).
static dongle_status_u _last_status = {0};

// Set true whenever a valid dongle packet arrives; refreshes the link watchdog.
static volatile bool _wlan_rx_seen = false;

static volatile bool _wlan_running = false;

// Wi-Fi association state machine.
typedef enum
{
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED
} wifi_state_t;

static wifi_state_t _current_state = WIFI_IDLE;
static absolute_time_t _timeout_time;

static bool _wlan_hal_is_wlan_connected(void)
{
    return (_current_state == WIFI_CONNECTED);
}

// Map our internal report format to the dongle USB personality.
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

// Pick a fresh random session id (1..0xFFF) for a new session / reboot / reconnect.
static void _wlan_new_session(void)
{
    uint16_t sid = (uint16_t)(get_rand_64() & 0x0FFFu);
    if (sid == 0) sid = 1;

    _session.id   = sid;
    _session.mode = _wlan_core_params
                        ? _wlan_mode_from_format(_wlan_core_params->core_report_format)
                        : DONGLE_MODE_SWITCH;

    _ack_echo         = 0;
    _last_status.value = 0;
}

// Transmit a single fully-formed dongle_pkt_s to the dongle AP.
// Only ever called as a reply to an inbound dongle packet (see file header).
static void _wlan_send_pkt(dongle_pkt_s *pkt)
{
    if (!pcb) return;

    // Tag every gamepad packet with the active session and the outstanding ack.
    pkt->session = dongle_session_pack(&_session);
    pkt->ack     = _ack_echo;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(dongle_pkt_s), PBUF_RAM);
    if (!p) return;

    memcpy(p->payload, pkt, sizeof(dongle_pkt_s));
    udp_sendto(pcb, p, &ap_addr, DONGLE_WLAN_PORT);
    pbuf_free(p);
}

// Reply to a WAKE beacon with our WAKE body (session identity + USB vid/pid).
static void _wlan_send_wake(void)
{
    dongle_pkt_s tx;
    memset(&tx, 0, sizeof(tx));

    dongle_wake_s wake;
    memset(&wake, 0, sizeof(wake));
    wake.session = _session;

    if (_wlan_core_params && _wlan_core_params->hid_device)
    {
        wake.vid = _wlan_core_params->hid_device->vid;
        wake.pid = _wlan_core_params->hid_device->pid;
    }

    tx.id  = DONGLE_PID_WAKE;
    tx.len = sizeof(dongle_wake_s);
    memcpy(tx.data, &wake, sizeof(wake));

    _wlan_send_pkt(&tx);
}

// Reply to STATUS / CORE_RELIABLE with the current input report for our mode.
// Falls back to an ack-only reply (len == 0) when no report is available, which
// still echoes the reliable ack and keeps the dongle's pacing alive.
static void _wlan_send_input(void)
{
    dongle_pkt_s tx;
    memset(&tx, 0, sizeof(tx));
    tx.id = DONGLE_PID_CORE_UNRELIABLE;

    core_report_s report = {0};
    if (core_get_generated_report(&report))
    {
        uint16_t n = report.size;
        if (n > sizeof(tx.data)) n = sizeof(tx.data);
        memcpy(tx.data, report.data, n);
        tx.len = n;
    }

    _wlan_send_pkt(&tx);
}

// Apply a freshly received dongle status (rumble, brake, connection, player led).
static void _wlan_apply_status(const dongle_status_u *st)
{
    if (st->rumble.left  != _last_status.rumble.left  ||
        st->rumble.right != _last_status.rumble.right ||
        st->brake.left   != _last_status.brake.left   ||
        st->brake.right  != _last_status.brake.right)
    {
        tp_evt_s evt = {
            .evt = TP_EVT_ERMRUMBLE,
            .evt_ermrumble = {
                .left       = st->rumble.left,
                .right      = st->rumble.right,
                .leftbrake  = st->brake.left,
                .rightbrake = st->brake.right
            }
        };
        transport_evt_cb(evt);
    }

    if (st->connection != _last_status.connection)
    {
        tp_evt_s evt = {
            .evt = TP_EVT_CONNECTIONCHANGE,
            .evt_connectionchange = {
                .connection = (st->connection == DONGLE_CONN_CONNECTED)
                                  ? TP_CONNECTION_CONNECTED
                                  : TP_CONNECTION_DISCONNECTED
            }
        };
        transport_evt_cb(evt);
    }

    if (st->player_number != _last_status.player_number)
    {
        tp_evt_s evt = {
            .evt = TP_EVT_PLAYERLED,
            .evt_playernumber = { .player_number = st->player_number }
        };
        transport_evt_cb(evt);
    }

    _last_status.value = st->value;
}

// lwIP UDP receive callback. Runs in lwIP/background context. This is the ONLY
// place gamepad UDP is transmitted: exactly one reply per dongle datagram.
static void _wlan_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                                   const ip_addr_t *addr, u16_t port)
{
    (void)arg; (void)upcb; (void)addr; (void)port;

    if (!p) return;

    bool valid = (p->tot_len == sizeof(dongle_pkt_s));

    dongle_pkt_s rx;
    if (valid)
        pbuf_copy_partial(p, &rx, sizeof(dongle_pkt_s), 0);

    pbuf_free(p);

    if (!valid) return;

    // Any valid dongle packet refreshes the link watchdog.
    _wlan_rx_seen = true;

    // Latch the reliable ack token so our reply echoes it (see protocol §6).
    if (rx.ack != 0)
        _ack_echo = rx.ack;

    switch ((dongle_pid_t)rx.id)
    {
        case DONGLE_PID_WAKE:
            // Empty beacon while link down -> reply with our WAKE body.
            if (rx.len == 0)
                _wlan_send_wake();
            break;

        case DONGLE_PID_STATUS:
            if (rx.len >= sizeof(dongle_status_u))
            {
                dongle_status_u st;
                memcpy(&st, rx.data, sizeof(dongle_status_u));
                _wlan_apply_status(&st);
            }
            _wlan_send_input();
            break;

        case DONGLE_PID_CORE_RELIABLE:
            // Host OUT / feature data tunneled to the active core.
            if (rx.len && _wlan_core_params && _wlan_core_params->core_report_tunnel)
                _wlan_core_params->core_report_tunnel(rx.data, rx.len);
            _wlan_send_input();
            break;

        default:
            // Anything else (incl. CORE_UNRELIABLE) is not answered.
            break;
    }
}

// Returns a pointer to a static buffer containing the SSID.
// Note: Not thread-safe, but fine for the single bring-up call.
static const char *_wlan_get_formatted_ssid(uint16_t value)
{
    static char ssid_buffer[32];
    snprintf(ssid_buffer, sizeof(ssid_buffer), "%s%04u", WIFI_SSID_BASE, value);
    return ssid_buffer;
}

// --- Wi-Fi association ---
static void _wlan_hal_on_connect_finish(bool success)
{
    if (success)
    {
        cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
        _wlan_rx_seen = true;
    }
    else
    {
        _current_state = WIFI_IDLE;
    }
}

static void _wlan_hal_connect_async(int timeout_seconds)
{
    _current_state = WIFI_CONNECTING;
    _timeout_time  = make_timeout_time_ms(timeout_seconds * 1000);

    cyw43_arch_wifi_connect_async(_wlan_get_formatted_ssid((uint16_t)WIFI_SSID_VALUE),
                                  WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);
}

static void _wlan_hal_poll_connection(void)
{
    if (_current_state == WIFI_IDLE)
        _wlan_hal_connect_async(10);

    if (_current_state != WIFI_CONNECTING)
        return;

    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (status == CYW43_LINK_UP)
    {
        cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
        _current_state = WIFI_CONNECTED;
        _wlan_hal_on_connect_finish(true);
    }
    else if (get_absolute_time() > _timeout_time || status < 0)
    {
        _current_state = WIFI_FAILED;
        _wlan_hal_on_connect_finish(false);
    }
}

static void _wlan_hal_init(void)
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
        return;

    cyw43_arch_enable_sta_mode();

    cyw43_wifi_set_roam_enabled(&cyw43_state, false);
    cyw43_wifi_set_interference_mode(&cyw43_state, CYW43_IFMODE_NONE);

    ipaddr_aton(AP_IP, &ap_addr);

    // Fresh session identity for this connection attempt.
    _wlan_new_session();

    if (pcb == NULL)
    {
        pcb = udp_new();
        // Bind the fixed local port; wait for the dongle before transmitting.
        err_t err = udp_bind(pcb, IP_ANY_TYPE, DONGLE_WLAN_PORT);
        if (err != ERR_OK)
            return;
        udp_recv(pcb, _wlan_receive_callback, NULL);
    }
}

static void _wlan_hal_deinit(void)
{
    tp_evt_s evt = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {.connection = TP_CONNECTION_DISCONNECTED},
    };
    transport_evt_cb(evt);

    if (pcb != NULL)
    {
        udp_remove(pcb);
        pcb = NULL;
    }

    cyw43_arch_deinit();
}

static void _wlan_hal_reset_connection(void)
{
    _current_state = WIFI_IDLE;

    tp_evt_s evt = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {.connection = TP_CONNECTION_DISCONNECTED}
    };
    transport_evt_cb(evt);

    _wlan_hal_deinit();
    _wlan_hal_init();
}

/***********************************************/
/********* Transport interface *****************/
void transport_wlan_stop(void)
{
    _wlan_hal_deinit();
    _wlan_core_params = NULL;
    _wlan_running     = false;
}

bool transport_wlan_init(core_params_s *params)
{
    _wlan_core_params = params;
    _wlan_hal_init();
    _wlan_running = true;
    return true;
}

void transport_wlan_task(uint64_t timestamp)
{
    if (!_wlan_running)
        return;

    if (_wlan_hal_is_wlan_connected())
    {
        // Link watchdog: only inbound dongle packets refresh it (respond-only
        // model means our replies are driven by the dongle, not a local timer).
        static interval_s reset_interval = {0};
        if (interval_resettable_run(timestamp, 5 * 1000 * 1000, _wlan_rx_seen, &reset_interval))
        {
            _wlan_hal_reset_connection();
            return;
        }
        if (_wlan_rx_seen)
            _wlan_rx_seen = false;

        // Pace IMU sampling from the main loop (safe context for blocking I/O).
        // The resulting samples feed the input report we build inside the RX
        // callback; we never transmit input from here.
        static interval_s gyro_interval = {0};
        if (_wlan_core_params && _wlan_core_params->sys_gyro_task &&
            interval_run(timestamp, 1000, &gyro_interval))
        {
            _wlan_core_params->sys_gyro_task();
        }
    }
    else
    {
        static interval_s check_itv = {0};
        if (interval_run(timestamp, 1000000, &check_itv))
            _wlan_hal_poll_connection();
    }
}

#endif
