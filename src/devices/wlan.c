#include <string.h>

#include "board_config.h"
#include "devices/wlan.h"
#include "devices_shared_types.h"
#include "hoja.h"

#if defined(HOJA_WLAN_DRIVER) && (HOJA_WLAN_DRIVER == WLAN_DRIVER_HAL)
    #include "hal/wlan_hal.h"
    #warning "HAL WLAN IN USE"
#endif

typedef void(*wlan_rxpacket_t)(uint8_t *data, uint16_t len);

typedef struct 
{
    wlan_rxpacket_t rxpacket_cb;
} transport_wlan_params_s;

typedef enum 
{
    WLAN_IN_REPORT_GET_INFO = 1,    // Used by the WLAN dongle to obtain info on current gamepad status
    WLAN_IN_REPORT_CORE_DATA,       // The dongle is passing through data meant for the current gamepad core
    WLAN_IN_REPORT_CORE_EVT,        // The dongle is passing through data meant for our generic core event callback
} wlan_report_id_t;

#define WLAN_PROTOCOL_VERSION 0x0001

typedef struct 
{
    uint16_t    protocol_version;
    uint8_t     gamepad_mode;   
    uint8_t     mac_address[6]; 
} wlan_gamepad_info_t;

wlan_gamepad_info_t _wlan_info = {.protocol_version=WLAN_PROTOCOL_VERSION};

void _wlan_send_packet(uint8_t *data, uint16_t len)
{
    // Logic here to send packet
}

void _wlan_send_info()
{
    const uint16_t len = sizeof(wlan_gamepad_info_t) + 1;
    uint8_t data[len];
    data[0] = WLAN_IN_REPORT_GET_INFO;
    memcpy(&data[1], &_wlan_info, sizeof(wlan_gamepad_info_t));
    _wlan_send_packet(data, len);
}

// Hoja WLAN input report callback
void wlan_input_cb(uint8_t *data, uint16_t len)
{
    if(len < 1) return;

    wlan_report_id_t wlan_report_id = (wlan_report_id_t)data[0];

    switch(wlan_report_id)
    {
        case WLAN_IN_REPORT_GET_INFO:
        break;

        case WLAN_IN_REPORT_CORE_DATA:
        break;

        case WLAN_IN_REPORT_CORE_EVT:
        break;
    }
}

void wlan_get_generated_report(uint8_t *data, uint16_t len)
{

}

bool wlan_mode_start(gamepad_mode_t mode, bool pairing_mode)
{
    #if defined(HOJA_WLAN_DRIVER) && (HOJA_WLAN_DRIVER>0)

        #if defined(HOJA_WLAN_INIT)
        return HOJA_WLAN_INIT(mode, pairing_mode);
        #else
        return false;
        #endif

    #endif
}

bool wlan_mode_task(uint64_t timestamp)
{
    #if defined(HOJA_WLAN_TASK)
    return HOJA_WLAN_TASK(timestamp);
    #endif

    return true;
}
