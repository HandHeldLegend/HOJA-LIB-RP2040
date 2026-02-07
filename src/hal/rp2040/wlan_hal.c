#include "board_config.h"

#if defined(HOJA_TRANSPORT_WLAN_DRIVER) && (HOJA_TRANSPORT_WLAN_DRIVER==WLAN_DRIVER_HAL)

#include <string.h>
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


volatile bool _wlan_connected = false;
struct udp_pcb* global_pcb = NULL;

size_t bufsize = PBUF_POOL_BUFSIZE;

typedef struct 
{
    uint8_t wlan_report_id;
    uint8_t report_format;
    uint8_t data[64];
    uint16_t len;
} hoja_wlan_report_s;

#define UDP_PORT 4444
#define BEACON_MSG_LEN_MAX sizeof(hoja_wlan_report_s)
#define BEACON_TARGET "255.255.255.255"

#define WIFI_SSID_BASE "HOJA_WLAN_"
#define WIFI_PASS "HOJA_1234"

typedef enum 
{
    HWLAN_REPORT_PASSTHROUGH = 0x01,
    HWLAN_REPORT_TRANSPORT = 0x02,
    HWLAN_REPORT_HELLO = 0x03,
} hoja_wlan_report_t;

core_params_s *_wlan_core_params = NULL;
struct udp_pcb* _wlan_pcb = NULL;

// Returns a pointer to a static buffer containing the SSID.
// Note: Not thread-safe, but perfect for a single-core Pico loop.
const char* _wlan_get_formatted_ssid(uint16_t value) {
    static char ssid_buffer[32];
    
    // %06u ensures it is exactly 6 digits (padded with 0s)
    // The value is masked with 1000000 if you want to strictly "trim" 
    // though a uint16_t max is 65535 (already < 6 digits).
    snprintf(ssid_buffer, sizeof(ssid_buffer), "%s%06u", WIFI_SSID_BASE, value);
    
    return ssid_buffer;
}

bool _wlan_hal_hid_tunnel(const void *report, uint16_t len)
{
    if(!global_pcb) global_pcb = udp_new();
    ip_addr_t addr;
    ipaddr_aton(BEACON_TARGET, &addr);
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, BEACON_MSG_LEN_MAX, PBUF_RAM);
    uint8_t *req = (uint8_t *)p->payload;
    //memset(req, 0, BEACON_MSG_LEN_MAX);
    memcpy(&req[0], report, len);
    err_t er = udp_sendto(global_pcb, p, &addr, UDP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
    return true;
}

void _wlan_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p) {
        hoja_wlan_report_s report = {0};
        memcpy(&report, p->payload, p->len);

        switch(report.wlan_report_id)
        {
            case HWLAN_REPORT_PASSTHROUGH:
            if(_wlan_core_params)
            {
                if(_wlan_core_params->core_report_tunnel)
                {
                    _wlan_core_params->core_report_tunnel(report.data, report.len);
                }
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
    if(_wlan_core_params)
    {
        hoja_wlan_report_s report = {.wlan_report_id=HWLAN_REPORT_HELLO};
        report.data[0] = _wlan_core_params->core_report_format;
        report.len = 1;

        _wlan_hal_hid_tunnel(&report, sizeof(hoja_wlan_report_s));
    }
}

void _wlan_hal_handle_disconnect() {
    if (_wlan_pcb != NULL) {
        udp_remove(_wlan_pcb); // This unbinds and frees the memory
        _wlan_pcb = NULL;
    }
    _wlan_connected = false;
}

void _wlan_hal_handle_async_connected()
{
    if(_wlan_pcb != NULL)
    {
        udp_remove(_wlan_pcb);
    }

    // Start UDP listening
    _wlan_pcb = udp_new();

    if(_wlan_pcb)
    {
        udp_bind(_wlan_pcb, IP_ANY_TYPE, UDP_PORT);
        udp_recv(_wlan_pcb, _wlan_receive_callback, NULL);
        _wlan_connected = true;
        _wlan_hal_send_hello();
    }
}

void _wlan_hal_async_connect()
{
    cyw43_arch_wifi_connect_async(_wlan_get_formatted_ssid(gamepad_config->wlan_pairing_key), WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);
}

volatile bool _wlan_running = false;

/***********************************************/
/********* Transport Defines *******************/
void transport_wlan_stop()
{
    if(_wlan_running)
        cyw43_arch_deinit();
    _wlan_core_params = NULL;
    _wlan_running = false;
}

bool transport_wlan_init(core_params_s *params)
{
    _wlan_core_params = params;

    // Init WLAN
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        return false;
    }
    _wlan_running = true;

    // Set power
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);

    // Enable station mode
    cyw43_arch_enable_sta_mode();
    return true;
}

void transport_wlan_task(uint64_t timestamp)
{
    static interval_s interval = {0};

    cyw43_arch_poll();

    if(interval_run(timestamp, 2000, &interval))
    {
        // Get status
        static int status = -1;
        int newstatus = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

        switch(newstatus)
        {
            case CYW43_LINK_UP:
            if(status == CYW43_LINK_DOWN)
            {
                // Connected callback
                _wlan_hal_handle_async_connected();
                status = newstatus;
            }
            break;

            case CYW43_LINK_DOWN:
            if(status == CYW43_LINK_UP)
            {
                // Perform reconnection
                _wlan_hal_handle_disconnect();
                _wlan_hal_async_connect();
                status = newstatus;
            }
            else if (status==-1) 
            {
                // Uninitialized, do connect
                status = newstatus;
            }
            break;

            case CYW43_LINK_BADAUTH:
            case CYW43_LINK_NONET:
            case CYW43_LINK_FAIL:
            if(status == CYW43_LINK_DOWN)
            {
                // Try reconnect
                _wlan_hal_handle_disconnect();
                _wlan_hal_async_connect();
            }
            break;
        }

        if(!_wlan_connected) return;

        core_report_s c_report = {0};
        hoja_wlan_report_s report = {.report_format = _wlan_core_params->core_report_format};
        if(core_get_generated_report(&c_report))
        {
            memcpy(report.data, c_report.data, c_report.size);
            report.len = c_report.size;
            _wlan_hal_hid_tunnel(&report, sizeof(hoja_wlan_report_s));
        }
    }
}

#endif