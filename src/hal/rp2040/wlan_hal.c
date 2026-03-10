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

#include "hal/sys_hal.h"

typedef enum 
{
    HWLAN_REPORT_CORE_UNRELIABLE = 0x01, 
    HWLAN_REPORT_CORE_RELIABLE = 0x02, 
    HWLAN_REPORT_STATUS_UNRELIABLE = 0x03, 
    HWLAN_REPORT_HELLO = 0x04,
} hoja_wlan_report_t;

typedef enum
{
    WLAN_CONNSTAT_IDLE = 0,
    WLAN_CONNSTAT_CONNECTED = 1,
    WLAN_CONNSTAT_DISCONNECTED = 2,
} hoja_wlan_connstat_t;

typedef enum 
{
    TRANSPORT_CONNSTAT_IDLE = 0,
    TRANSPORT_CONNSTAT_CONNECTED = 1,
    TRANSPORT_CONNSTAT_DISCONNECTED = 2,
} hoja_transport_connstat_t;

typedef struct
{
    uint8_t rumble_left;
    uint8_t rumble_right;
    uint8_t brake_left;
    uint8_t brake_right;
    uint8_t connection_status;
    uint8_t transport_status;
} hoja_wlan_status_s;

typedef struct __attribute__((packed))
{
    uint16_t session_sig; // RNG for the current gamepad/dongle session
    uint16_t gamepad_sig; // RNG for the last gamepad message
    uint16_t dongle_sig; // RNG for the last dongle message
    uint8_t wlan_report_id;
    uint8_t report_format;
    uint8_t data[64];
    uint16_t len;
} hoja_wlan_report_s;

#define WLAN_REPORT_SIZE sizeof(hoja_wlan_report_s)

#define UDP_PORT 4444
#define AP_IP "192.168.4.1"

#define WIFI_SSID_BASE "HOJA_WLAN_"
#define WIFI_PASS "HOJA_1234"

static struct udp_pcb *pcb = NULL;
static ip_addr_t ap_addr;

hoja_wlan_status_s _wlan_stat = {0};

core_params_s *_wlan_core_params = NULL;

volatile bool _wlan_send_hello = false;

// Define states for our connection tracker
typedef enum
{
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED
} wifi_state_t;

typedef struct 
{
    uint16_t session_sig;
    uint16_t gamepad_sig;
    uint16_t dongle_sig;
} wlan_sigs_s;

wlan_sigs_s _wlan_sigs = {0};

static wifi_state_t _current_state = WIFI_IDLE;
static absolute_time_t _timeout_time;
volatile int _ping_misses = 0;
volatile bool _hello_got = false;
uint32_t pm_mode = 0;

volatile bool _wlan_running = false;
volatile bool _wlan_timeout_reset = false;

uint16_t _wlan_status_rumblesig(hoja_wlan_status_s status)
{
    return (
        status.brake_left+status.brake_right
        +status.rumble_left+status.rumble_right
    );
}

uint16_t _wlan_status_connectionsig(hoja_wlan_status_s status)
{
    return (
        status.connection_status+status.transport_status
    );
}

static inline void _wlan_handle_new_status(hoja_wlan_status_s status)
{
    if(_wlan_status_rumblesig(status) != _wlan_status_rumblesig(_wlan_stat))
    {
        _wlan_stat.rumble_left  = status.rumble_left;
        _wlan_stat.rumble_right = status.rumble_right;
        _wlan_stat.brake_left   = status.brake_left;
        _wlan_stat.brake_right  = status.brake_right;

        tp_evt_s evt = {
            .evt = TP_EVT_ERMRUMBLE,
            .evt_ermrumble = {
                .left   = _wlan_stat.rumble_left,
                .right  = _wlan_stat.rumble_right,
                .leftbrake  = _wlan_stat.brake_left,
                .rightbrake = _wlan_stat.brake_right
            }
        };

        transport_evt_cb(evt);
    }

    if(_wlan_status_connectionsig(status) != _wlan_status_connectionsig(_wlan_stat))
    {
        _wlan_stat.connection_status = status.connection_status;
        _wlan_stat.transport_status = status.transport_status;

        tp_evt_s evt = {
            .evt = TP_EVT_CONNECTIONCHANGE,
            .evt_connectionchange = {
                .connection = _wlan_stat.connection_status
            }
        };

        transport_evt_cb(evt);
    }
}

bool _wlan_hal_raw_tunnel(hoja_wlan_report_s *report);

bool _wlan_hal_is_wlan_connected(void)
{
    return (_current_state == WIFI_CONNECTED);
}

void _wlan_hal_send_hello()
{
    hoja_wlan_report_s pkt;
    pkt.wlan_report_id = HWLAN_REPORT_HELLO;
    pkt.report_format = _wlan_core_params->core_report_format;
    pkt.len = 0;
    _wlan_hal_raw_tunnel(&pkt);
}

void _wlan_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if(!p) return;

    if(p->tot_len == sizeof(hoja_wlan_report_s))
    {
        _wlan_timeout_reset = true;

        hoja_wlan_report_s pkt;
        pbuf_copy_partial(p, &pkt, sizeof(hoja_wlan_report_s), 0);
        pbuf_free(p);

        _wlan_sigs.dongle_sig = pkt.dongle_sig;

        switch(pkt.wlan_report_id)
        {
            case HWLAN_REPORT_HELLO:
            _wlan_send_hello = true; 
            break;

            case HWLAN_REPORT_CORE_UNRELIABLE:
            case HWLAN_REPORT_CORE_RELIABLE:
            if (_wlan_core_params) 
            {
                if (_wlan_core_params->core_report_tunnel)
                {
                    _wlan_core_params->core_report_tunnel(pkt.data, pkt.len);
                }
            }
            break;

            case HWLAN_REPORT_STATUS_UNRELIABLE:
            hoja_wlan_status_s pstat;
            memcpy(&pstat, pkt.data, sizeof(hoja_wlan_status_s));
            _wlan_handle_new_status(pstat);
            break;
        }
    }
}

// This is called when the process finishes (Success or Failure)
void _wlan_hal_on_connect_finish(bool success)
{
    if (success)
    {
        cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
        _wlan_timeout_reset = true;
    }
    else
    {
        //printf("Callback: Connection Timed Out or Failed.\n");
        _current_state = WIFI_IDLE;
    }
}

// Returns a pointer to a static buffer containing the SSID.
// Note: Not thread-safe, but perfect for a single-core Pico loop.
const char *_wlan_get_formatted_ssid(uint16_t value)
{
    static char ssid_buffer[32];

    // %06u ensures it is exactly 4 digits (padded with 0s)
    // The value is masked with 1000000 if you want to strictly "trim"
    // though a uint16_t max is 65535 (already < 4 digits).
    snprintf(ssid_buffer, sizeof(ssid_buffer), "%s%04u", WIFI_SSID_BASE, value);

    return ssid_buffer;
}

bool _wlan_hal_raw_tunnel(hoja_wlan_report_s *report)
{
    // Generate random gamepad sig
    _wlan_sigs.gamepad_sig = get_rand_64() & 0xFFFF;

    // Copy sigs
    report->session_sig = _wlan_sigs.session_sig;
    report->gamepad_sig = _wlan_sigs.gamepad_sig;
    report->dongle_sig  = _wlan_sigs.dongle_sig;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(hoja_wlan_report_s), PBUF_RAM);
    if (!p) return false;

    memcpy(p->payload, report, sizeof(hoja_wlan_report_s));

    err_t er = udp_sendto(pcb, p, &ap_addr, UDP_PORT);
    pbuf_free(p);
    return true;
}

bool _wlan_hal_core_report_tunnel(core_report_s *report)
{
    hoja_wlan_report_s pkt;

    memcpy(pkt.data, report->data, 64);
    pkt.wlan_report_id = HWLAN_REPORT_CORE_UNRELIABLE;
    pkt.report_format = report->reportformat;
    pkt.len = report->size;

    return _wlan_hal_raw_tunnel(&pkt);
}

// --- Initializer ---
void _wlan_hal_connect_async(int timeout_seconds)
{
    //printf("Attempting Async Connect...\n");

    _current_state = WIFI_CONNECTING;
    _timeout_time = make_timeout_time_ms(timeout_seconds * 1000);

    // Start the hardware-level async connect
    cyw43_arch_wifi_connect_async(_wlan_get_formatted_ssid((uint16_t) 1234), WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);
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

void _wlan_hal_deinit()
{
    tp_evt_s evt = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {.connection=TP_CONNECTION_DISCONNECTED},
    };
    transport_evt_cb(evt);

    if(pcb != NULL)
    {
        udp_remove(pcb);
        pcb = NULL;
    }

    cyw43_arch_deinit();
}

void _wlan_hal_init()
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA))
    {
        //printf("failed to initialise\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    cyw43_wifi_set_roam_enabled(&cyw43_state, false);
    cyw43_wifi_set_interference_mode(&cyw43_state, CYW43_IFMODE_NONE);

    ipaddr_aton(AP_IP, &ap_addr);

    // Create fresh session signature
    _wlan_sigs.session_sig = get_rand_64() & 0xFFFF;

    if(pcb==NULL)
    {
        pcb = udp_new();
        // Bind to a fixed local port so the host knows where to reply
        err_t err = udp_bind(pcb, IP_ANY_TYPE, UDP_PORT);
        if (err != ERR_OK) {
            return;
        }
        udp_recv(pcb, _wlan_receive_callback, NULL);
    }
}



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

void _wlan_hal_reset_connection(void) {
    // Optional: Send a deinit to the hardware if you want a hard reset
    // cyw43_arch_wifi_disconnect_async(); 
    
    _current_state = WIFI_IDLE;
    _hello_got = false;
    _wlan_send_hello = false;
    
    // Notify the transport layer that we are disconnected
    tp_evt_s evt = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {.connection = TP_CONNECTION_DISCONNECTED}
    };
    transport_evt_cb(evt);

    _wlan_hal_deinit();
    _wlan_hal_init();
}

void transport_wlan_task(uint64_t timestamp)
{
    if (!_wlan_running)
        return;

    if (_wlan_hal_is_wlan_connected())
    {
        static interval_s reset_interval = {0};
        if(interval_resettable_run(timestamp, 5*1000*1000, _wlan_timeout_reset, &reset_interval))
        {
            _wlan_hal_reset_connection();
            return;
        }
        if(_wlan_timeout_reset) _wlan_timeout_reset = false;

        static interval_s poll_interval = {0};
        if (interval_run(timestamp, 2000, &poll_interval))
        {
            if(_wlan_send_hello)
            {
                _wlan_send_hello = false;
                _wlan_hal_send_hello();
            }
            else 
            {
                core_report_s c_report = {0};
                if (core_get_generated_report(&c_report))
                {
                    _wlan_hal_core_report_tunnel(&c_report);
                    if (_wlan_core_params->sys_gyro_task)
                        _wlan_core_params->sys_gyro_task();
                }
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