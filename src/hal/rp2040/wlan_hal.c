#include "board_config.h"

#if defined(HOJA_TRANSPORT_WLAN_DRIVER) && (HOJA_TRANSPORT_WLAN_DRIVER==WLAN_DRIVER_HAL)

#include "hal/wlan_hal.h"
#include "cores/cores.h"
#include "transport/transport.h"
#include "transport/transport_wlan.h"

#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "hardware/watchdog.h"

#include "utilities/interval.h"

#define UDP_PORT 4444
#define BEACON_MSG_LEN_MAX 64
#define BEACON_TARGET "255.255.255.255"
#define BEACON_INTERVAL_MS 1

#define WIFI_SSID "HOJA_WLAN_DONGLE"
#define WIFI_PASS "HOJA_1234"

volatile bool _wlan_connected = false;
struct udp_pcb* global_pcb = NULL;

size_t bufsize = PBUF_POOL_BUFSIZE;

core_params_s *_wlan_core_params = NULL;


bool _wlan_hal_hid_tunnel(uint8_t report_id, const void *report, uint16_t len)
{
    if(!global_pcb) global_pcb = udp_new();
    ip_addr_t addr;
    ipaddr_aton(BEACON_TARGET, &addr);

    //cyw43_thread_lock_check

    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, BEACON_MSG_LEN_MAX, PBUF_RAM);
    uint8_t *req = (uint8_t *)p->payload;
    //memset(req, 0, BEACON_MSG_LEN_MAX);
    req[0] = report_id;
    memcpy(&req[1], report, len);
    err_t er = udp_sendto(global_pcb, p, &addr, UDP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();

    return true;
}

typedef struct
{
  uint8_t report_id;
  uint8_t data[63];
} sinputreport_s;

void _wlan_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p) {
        uint8_t data[64] = {0};
        memcpy(data, p->payload, p->len);

        if(_wlan_core_params)
        {
            if(_wlan_core_params->report_tunnel)
            {
                _wlan_core_params->report_tunnel(data, p->len);
            }
        }

        pbuf_free(p);
    }
}

bool wlan_hal_init(int device_mode, bool pairing_mode)
{
    // Init WLAN
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        return false;
    }

    // Set power
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);

    // Enable station mode
    cyw43_arch_enable_sta_mode();

    
    watchdog_disable();
    // Connect to AP
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        // Failed to connect
        watchdog_enable(5000, false);
        return false;
    } else {
        // Connected
        watchdog_enable(5000, false);

        // Start UDP listening
        struct udp_pcb* pcb = udp_new();
        udp_bind(pcb, IP_ANY_TYPE, UDP_PORT);
        udp_recv(pcb, _wlan_receive_callback, NULL);

        _wlan_connected = true;
    }

    return true;
}

bool wlan_hal_task(uint64_t timestamp)
{
    if(!_wlan_connected) return true;

    static interval_s interval = {0};

    cyw43_arch_poll();

    if(interval_run(timestamp, 2000, &interval))
    {
        sinput_hid_report(timestamp, _wlan_hal_hid_tunnel);
        return true;
    }

    return false;

    //static interval_s interval_w = {0};
    //if(interval_run(timestamp, 16000, &interval_w))
    //{
    //    cyw43_arch_poll();
    //}
}

/***********************************************/
/********* Transport Defines *******************/
void transport_wlan_stop()
{

}

bool transport_wlan_init(core_params_s *params)
{
    _wlan_core_params = params;
}

void transport_wlan_task(uint64_t timestamp)
{

}

#endif