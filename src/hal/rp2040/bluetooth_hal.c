#include "board_config.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_HAL)

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_chipset_cyw43.h"
#include "btstack_config.h"
#include "btstack.h"
#include <string.h>

#include "devices/bluetooth.h"

static const char hid_device_name[] = "Wireless Gamepad";
static const char service_name[] = "Wireless Gamepad";
static uint8_t hid_service_buffer[250];
static uint8_t device_id_sdp_service_buffer[100];
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t hid_cid;

bool _bluetooth_hal_hid_tunnel(uint8_t report_id, const void *report, uint16_t len)
{
    static uint8_t new_report[64] = {0};
    new_report[0] = report_id;
    memcpy(&(new_report[1]), report, len);

    if(hid_cid)
        hid_device_send_interrupt_message(hid_cid, new_report, len+1);
}

void bluetooth_hal_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb)
{

}

void bluetooth_hal_stop()
{

}

void bluetooth_hal_task(uint32_t timestamp)
{

}

uint32_t bluetooth_hal_get_fwversion()
{
    
}

#endif