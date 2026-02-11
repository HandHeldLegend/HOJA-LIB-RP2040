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

#include "hardware/watchdog.h"

#include "utilities/interval.h"
#include "utilities/settings.h"

struct udp_pcb *_wlan_tx_pcb = NULL;
struct udp_pcb *_wlan_rx_pcb = NULL;

typedef struct
{
    uint8_t wlan_report_id;
    uint8_t report_format;
    uint8_t data[64];
    uint16_t len;
} hoja_wlan_report_s;

typedef struct
{
    uint8_t device_mac[6];
    char device_name[32];
} hoja_wlan_hello_s;

#define UDP_PORT 4444
#define BEACON_MSG_LEN_MAX sizeof(hoja_wlan_report_s)
#define BEACON_TARGET "255.255.255.255"
#define PING_MISS_TIMEOUT 8
#define PING_CHECK_TIME_MS 500

#define WIFI_SSID_BASE "HOJA_WLAN_"
#define WIFI_PASS "HOJA_1234"

typedef enum
{
    HWLAN_REPORT_PASSTHROUGH = 0x01,
    HWLAN_REPORT_TRANSPORT = 0x02,
    HWLAN_REPORT_HELLO = 0x03,
    HWLAN_REPORT_PING = 0x04,
} hoja_wlan_report_t;

core_params_s *_wlan_core_params = NULL;

// Define states for our connection tracker
typedef enum
{
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED
} wifi_state_t;

static wifi_state_t _current_state = WIFI_IDLE;
static absolute_time_t _timeout_time;
volatile int _ping_misses = 0;

bool _wlan_hal_raw_tunnel(hoja_wlan_report_s *report);

bool _wlan_hal_is_wlan_connected(void)
{
    return _current_state == WIFI_CONNECTED;
}

void _wlan_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if (p)
    {
        if (p->len != sizeof(hoja_wlan_report_s))
            return;

        hoja_wlan_report_s report = {0};
        memcpy(&report, p->payload, p->len);

        switch (report.wlan_report_id)
        {
        case HWLAN_REPORT_PASSTHROUGH:
            if (_wlan_core_params)
            {
                if (_wlan_core_params->core_report_tunnel)
                {
                    _wlan_core_params->core_report_tunnel(report.data, report.len);
                }
            }
            break;

        case HWLAN_REPORT_PING:
        {
            _ping_misses = 0;
            hoja_set_connected_status(CONN_STATUS_PLAYER_1);
        }
        break;

        case HWLAN_REPORT_TRANSPORT:
            tp_evt_s evt = {0};
            memcpy(&evt, report.data, sizeof(tp_evt_s));
            transport_evt_cb(evt);
            break;

        default:
            // Unsupported
            break;
        }
        pbuf_free(p);
    }
}

void _wlan_hal_send_hello()
{
    if (_wlan_core_params)
    {
        hoja_wlan_report_s report = {.wlan_report_id = HWLAN_REPORT_HELLO};
        report.report_format = _wlan_core_params->core_report_format;
        report.len = 0;

        _wlan_hal_raw_tunnel(&report);
    }
}

// This is called when the process finishes (Success or Failure)
void _wlan_hal_on_connect_finish(bool success)
{
    if (success)
    {
        printf("Callback: Connection Established!\n");
        if (_wlan_rx_pcb)
        {
            udp_remove(_wlan_rx_pcb);
            _wlan_rx_pcb = NULL;
        }

        // Start UDP listening
        _wlan_rx_pcb = udp_new();

        if (_wlan_rx_pcb)
        {
            udp_bind(_wlan_rx_pcb, IP_ANY_TYPE, UDP_PORT);
            udp_recv(_wlan_rx_pcb, _wlan_receive_callback, NULL);
        }

        _wlan_hal_send_hello();
    }
    else
    {
        printf("Callback: Connection Timed Out or Failed.\n");
        _current_state = WIFI_IDLE;
    }
}

// Returns a pointer to a static buffer containing the SSID.
// Note: Not thread-safe, but perfect for a single-core Pico loop.
const char *_wlan_get_formatted_ssid(uint16_t value)
{
    static char ssid_buffer[32];

    // %06u ensures it is exactly 6 digits (padded with 0s)
    // The value is masked with 1000000 if you want to strictly "trim"
    // though a uint16_t max is 65535 (already < 6 digits).
    snprintf(ssid_buffer, sizeof(ssid_buffer), "%s%06u", WIFI_SSID_BASE, value);

    return ssid_buffer;
}

static hoja_wlan_report_s _wlan_hal_raw_tunnel_out;
bool _wlan_hal_raw_tunnel(hoja_wlan_report_s *report)
{
    if (!_wlan_tx_pcb)
        _wlan_tx_pcb = udp_new();

    ip_addr_t addr;
    ipaddr_aton(BEACON_TARGET, &addr);

    memcpy(&_wlan_hal_raw_tunnel_out, report, sizeof(hoja_wlan_report_s));
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(hoja_wlan_report_s), PBUF_REF);
    if (!p) return false;

    p->payload = &_wlan_hal_raw_tunnel_out;

    err_t er = udp_sendto(_wlan_tx_pcb, p, &addr, UDP_PORT);
    pbuf_free(p);
    return true;
}

static hoja_wlan_report_s _wlan_hal_core_report_tunnel_out;
bool _wlan_hal_core_report_tunnel(core_report_s *report)
{
    if (!_wlan_tx_pcb)
        _wlan_tx_pcb = udp_new();

    ip_addr_t addr;
    ipaddr_aton(BEACON_TARGET, &addr);

    memcpy(_wlan_hal_core_report_tunnel_out.data, report->data, 64);
    _wlan_hal_core_report_tunnel_out.wlan_report_id = HWLAN_REPORT_PASSTHROUGH;
    _wlan_hal_core_report_tunnel_out.report_format = report->reportformat;
    _wlan_hal_core_report_tunnel_out.len = report->size;
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(hoja_wlan_report_s), PBUF_REF);
    if(!p) return false;

    p->payload = &_wlan_hal_core_report_tunnel_out;

    err_t er = udp_sendto(_wlan_tx_pcb, p, &addr, UDP_PORT);
    pbuf_free(p);
    return true;
}

// --- Initializer ---
void _wlan_hal_connect_async(int timeout_seconds)
{
    printf("Attempting Async Connect...\n");

    _current_state = WIFI_CONNECTING;
    _timeout_time = make_timeout_time_ms(timeout_seconds * 1000);

    // Start the hardware-level async connect
    cyw43_arch_wifi_connect_async("HOJA_WLAN_1234", WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);
}

void _wlan_hal_poll_connection(void)
{
    if (_current_state == WIFI_IDLE)
        _wlan_hal_connect_async(10);

    if (_current_state != WIFI_CONNECTING)
        return;

    // Check hardware status
    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (status == CYW43_LINK_UP)
    {
        _current_state = WIFI_CONNECTED;
        _wlan_hal_on_connect_finish(true);
    }
    else if (get_absolute_time() > _timeout_time || status < 0)
    {
        _current_state = WIFI_FAILED;
        _wlan_hal_on_connect_finish(false);
    }
}

void _wlan_hal_deinit()
{
    hoja_set_connected_status(CONN_STATUS_DISCONNECTED);
    cyw43_arch_deinit();
}

void _wlan_hal_init()
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
    {
        printf("failed to initialise\n");
        return;
    }

    cyw43_arch_enable_sta_mode();
    
    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
}

volatile bool _wlan_running = false;

/***********************************************/
/********* Transport Defines *******************/
void transport_wlan_stop()
{
    _wlan_hal_deinit();
    _wlan_core_params = NULL;
    _wlan_running = false;
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
        static interval_s poll_interval = {0};
        if (interval_run(timestamp, 2000, &poll_interval))
        {
            core_report_s c_report = {0};
            if (core_get_generated_report(&c_report))
            {
                _wlan_hal_core_report_tunnel(&c_report);
                if (_wlan_core_params->sys_gyro_task)
                    _wlan_core_params->sys_gyro_task();
            }
        }

        static interval_s ping_interval = {0};
        if(interval_run(timestamp, PING_CHECK_TIME_MS*1000, &ping_interval))
        {
            _ping_misses++;

            if(_ping_misses > PING_MISS_TIMEOUT)
            {
                _wlan_hal_deinit();
                _wlan_hal_init();

                _current_state = WIFI_IDLE;
                _ping_misses = 0;
            }
        }
    }

    else
    {
        static interval_s check_itv = {0};
        if (interval_run(timestamp, 1000000, &check_itv))
        {
            _wlan_hal_poll_connection();
        }
    }
}

#endif