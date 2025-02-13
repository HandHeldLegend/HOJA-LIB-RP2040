#include "board_config.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_HAL)

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_chipset_cyw43.h"
#include "btstack_config.h"
#include "btstack.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include <string.h>

#include "devices/bluetooth.h"
#include "switch/switch_commands.h"
#include "usb/swpro.h"

static const char hid_device_name[] = "Wireless Gamepad";
static const char service_name[] = "Wireless Gamepad";
static uint8_t hid_service_buffer[250];
static uint8_t pnp_service_buffer[200];
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

static void _bt_hid_report_handler(uint16_t cid,
    hid_report_type_t report_type,
    uint16_t report_id,
    int report_size, uint8_t *report) {

    if(cid == hid_cid)
    {
        if(report_id == SW_OUT_ID_RUMBLE)
        {
            switch_haptics_rumble_translate(&report[2]);
        }
        else
        {
            switch_commands_future_handle(report[0], report, report_size);
        }
    }
}

static void _bt_hal_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    uint8_t status;
     if(packet_type == HCI_EVENT_PACKET)
    {
        switch (packet[0]){
            case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
                    break;

            case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    log_info("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", hci_event_user_confirmation_request_get_numeric_value(packet));
                    log_info("SSP User Confirmation Auto accept\n");
                    break;
            case HCI_EVENT_HID_META:
                switch (hci_event_hid_meta_get_subevent_code(packet)){
                    case HID_SUBEVENT_CONNECTION_OPENED:
                        status = hid_subevent_connection_opened_get_status(packet);
                        if (status) {
                            // outgoing connection failed
                            printf("Connection failed, status 0x%x\n", status);
                            hid_cid = 0;
                            return;
                        }
                        hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                        log_info("HID Connected\n");
                        hid_device_request_can_send_now_event(hid_cid);
                        break;
                    case HID_SUBEVENT_CONNECTION_CLOSED:
                        log_info("HID Disconnected\n");
                        hid_cid = 0;
                        break;
                    case HID_SUBEVENT_CAN_SEND_NOW:  
                        if(hid_cid!=0){ //Solves crash when disconnecting gamepad on android
                          swpro_hid_report(0, _bt_hal_packet_handler);
                          hid_device_request_can_send_now_event(hid_cid);
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
}

void bluetooth_hal_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb)
{
    bd_addr_t newAddr = {0x7c,
        0xbb,
        0x8a,
        (uint8_t)(get_rand_32() % 0xff),
        (uint8_t)(get_rand_32() % 0xff),
        (uint8_t)(get_rand_32() % 0xff)};

    // If the init fails it returns true lol
    if (cyw43_arch_init()) {
        return;
    }

    gap_discoverable_control(1);
    gap_set_class_of_device(0x2508);
    gap_set_local_name("Pro Controller");
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_ROLE_SWITCH |
                                        LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_set_allow_role_switch(true);

    hci_set_chipset(btstack_chipset_cyw43_instance());
    hci_set_bd_addr(newAddr);

    // L2CAP
  l2cap_init();
  sm_init();
  // SDP Server
  sdp_init();

  hid_sdp_record_t hid_sdp_record = {0x2508,
                                     33,
                                     1,
                                     1,
                                     1,
                                     0,
                                     0,
                                     0xFFFF,
                                     0xFFFF,
                                     3200,
                                     swpro_hid_report_descriptor,
                                     203,
                                     hid_device_name};

    // Register SDP services
    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));

    hid_create_sdp_record(hid_service_buffer, 0x10000, &hid_sdp_record);

    create_sdp_hid_record(hid_service_buffer, &hid_sdp_record);
    sdp_register_service(hid_service_buffer);

    memset(pnp_service_buffer, 0, sizeof(pnp_service_buffer));
    create_sdp_pnp_record(pnp_service_buffer, DEVICE_ID_VENDOR_ID_SOURCE_USB,
                        0x057E, 0x2009, 0x0001);
    sdp_register_service(pnp_service_buffer);

    // HID Device
    hid_device_init(1, 203,
        swpro_hid_report_descriptor);


    hci_event_callback_registration.callback = &_bt_hal_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hid_device_register_packet_handler(&_bt_hal_packet_handler);
    hid_device_register_packet_handler(&_bt_hid_report_handler);

    hci_power_control(HCI_POWER_ON);
}

void bluetooth_hal_stop()
{

}

void bluetooth_hal_task(uint32_t timestamp)
{
    btstack_run_loop_embedded_execute_once();
}

uint32_t bluetooth_hal_get_fwversion()
{
    
}

#endif