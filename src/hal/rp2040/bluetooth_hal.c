#include "hal/bluetooth_hal.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_HAL)

#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_chipset_cyw43.h"
#include "btstack_config.h"

#include "btstack.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "btstack_tlv.h"

#include <string.h>

#include "switch/switch_commands.h"
#include "utilities/settings.h"
#include "utilities/static_config.h"
#include "utilities/interval.h"
#include "usb/swpro.h"
#include "usb/sinput.h"
#include "hal/sys_hal.h"

#include "hoja.h"

#define BT_HAL_TARGET_POLLING_RATE_MS 6

volatile bool _connected = false;
volatile uint64_t _bt_hal_timer = 0;

static const uint8_t switch_bt_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x06, 0x01, 0xFF,  //   Usage Page (Vendor Defined 0xFF01)

    0x85, 0x21,  //   Report ID (33)
    0x09, 0x21,  //   Usage (0x21)
    0x75, 0x08,  //   Report Size (8)
    0x95, 0x30,  //   Report Count (48)
    0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                 //   Position)

    0x85, 0x30,  //   Report ID (48)
    0x09, 0x30,  //   Usage (0x30)
    0x75, 0x08,  //   Report Size (8)
    0x95, 0x30,  //   Report Count (48)
    0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                 //   Position)

    0x85, 0x31,        //   Report ID (49)
    0x09, 0x31,        //   Usage (0x31)
    0x75, 0x08,        //   Report Size (8)
    0x96, 0x69, 0x01,  //   Report Count (361)
    0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                 //   Position)

    0x85, 0x32,        //   Report ID (50)
    0x09, 0x32,        //   Usage (0x32)
    0x75, 0x08,        //   Report Size (8)
    0x96, 0x69, 0x01,  //   Report Count (361)
    0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                 //   Position)

    0x85, 0x33,        //   Report ID (51)
    0x09, 0x33,        //   Usage (0x33)
    0x75, 0x08,        //   Report Size (8)
    0x96, 0x69, 0x01,  //   Report Count (361)
    0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                 //   Position)

    0x85, 0x3F,  //   Report ID (63)
    0x05, 0x09,  //   Usage Page (Button)
    0x19, 0x01,  //   Usage Minimum (0x01)
    0x29, 0x10,  //   Usage Maximum (0x10)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x01,  //   Logical Maximum (1)
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x10,  //   Report Count (16)
    0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                 //   Position)
    0x05, 0x01,  //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,  //   Usage (Hat switch)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x07,  //   Logical Maximum (7)
    0x75, 0x04,  //   Report Size (4)
    0x95, 0x01,  //   Report Count (1)
    0x81, 0x42,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null
                 //   State)
    0x05, 0x09,  //   Usage Page (Button)
    0x75, 0x04,  //   Report Size (4)
    0x95, 0x01,  //   Report Count (1)
    0x81, 0x01,  //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No
                 //   Null Position)
    0x05, 0x01,  //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,  //   Usage (X)
    0x09, 0x31,  //   Usage (Y)
    0x09, 0x33,  //   Usage (Rx)
    0x09, 0x34,  //   Usage (Ry)
    0x16, 0x00, 0x00,              //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65534)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x04,                    //   Report Count (4)
    0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                 //   Position)
    0x06, 0x01, 0xFF,  //   Usage Page (Vendor Defined 0xFF01)

    0x85, 0x01,  //   Report ID (1)
    0x09, 0x01,  //   Usage (0x01)
    0x75, 0x08,  //   Report Size (8)
    0x95, 0x30,  //   Report Count (48)
    0x91, 0x02,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                 //   Null Position,Non-volatile)

    0x85, 0x10,  //   Report ID (16)
    0x09, 0x10,  //   Usage (0x10)
    0x75, 0x08,  //   Report Size (8)
    0x95, 0x09,  //   Report Count (9)
    0x91, 0x02,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                 //   Null Position,Non-volatile)

    0x85, 0x11,  //   Report ID (17)
    0x09, 0x11,  //   Usage (0x11)
    0x75, 0x08,  //   Report Size (8)
    0x95, 0x30,  //   Report Count (48)
    0x91, 0x02,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                 //   Null Position,Non-volatile)

    0x85, 0x12,  //   Report ID (18)
    0x09, 0x12,  //   Usage (0x12)
    0x75, 0x08,  //   Report Size (8)
    0x95, 0x30,  //   Report Count (48)
    0x91, 0x02,  //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                 //   Null Position,Non-volatile)
    0xC0,        // End Collection
};

static const char hid_device_name[] = "Wireless Gamepad";
static const char service_name[] = "Wireless Gamepad";
static uint8_t hid_service_buffer[700]      = {0};
static uint8_t pnp_service_buffer[700]      = {0};
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t hid_cid = 0;

static uint32_t last_hid_report_timestamp_ms = 0;
static interval_s hid_report_interval = {0};
// Timer structure for scheduling delayed HID report requests
static btstack_timer_source_t hid_timer;

// Compare addresses and return true if they are the same
bool _compare_mac_addr(const bd_addr_t addr1, const bd_addr_t addr2)
{
    for (int i = 0; i < 6; i++)
    {
        if (addr1[i] != addr2[i])
        {
            return false;
        }
    }
    return true;
}

bool _bluetooth_hal_hid_tunnel(uint8_t report_id, const void *report, uint16_t len)
{
    uint8_t new_report[64] = {0};
    new_report[0] = 0xA1; // Type of input report
    new_report[1] = report_id;
    memcpy(&(new_report[2]), report, len);

    if (hid_cid)
    {
        hid_device_send_interrupt_message(hid_cid, new_report, len+2);
    }

    return true;
}

static void _bt_hid_report_handler(uint16_t cid,
                                   hid_report_type_t report_type,
                                   uint16_t report_id,
                                   int report_size, uint8_t *report)
{
    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
        if (cid == hid_cid)
        {
            printf("REPORT? %x, c: %d\n", report_id, hid_cid);
            uint8_t tmp[64] = {0};

            tmp[0] = report_id;

            switch(hoja_get_status().gamepad_mode)
            {
                default:
                if (report_id == SW_OUT_ID_RUMBLE)
                {
                    switch_haptics_rumble_translate(&report[1]);
                }
                else if (report_id == SW_OUT_ID_RUMBLE_CMD)
                {
                    //switch_haptics_rumble_translate(&report[1]);
                    memcpy(&tmp[1], report, report_size);
                    switch_commands_future_handle(report_id, tmp, report_size + 1);
                }
                break;

                case GAMEPAD_MODE_SINPUT:
                if(report_id == REPORT_ID_SINPUT_OUTPUT)
                {
                    sinput_hid_handle_command(report);
                }
                break;
            }

        }
    }
}

volatile bool _pairing_mode = false;

// Timer handler to request can send now event after delay
static void hid_timer_handler(btstack_timer_source_t *ts) {
    // Avoid compiler warning for unused parameter
    (void)ts;
    
    // Request another CAN_SEND_NOW event
    hid_device_request_can_send_now_event(hid_cid);
}

static void _bt_hal_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t packet_size)
{
    UNUSED(channel);
    UNUSED(packet_size);
    uint8_t status;
    if (packet_type == HCI_EVENT_PACKET)
    {
        switch (packet[0])
        {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
                return;

            if(!hid_cid)
            {
                hci_set_bd_addr(gamepad_config->switch_mac_address);

                if(_pairing_mode)
                {
                    gap_delete_all_link_keys();
                    gap_discoverable_control(1);
                }
                else 
                {
                    if(gamepad_config->host_mac_switch[0] != 0xFF && gamepad_config->host_mac_switch[1] != 0xFF)
                    {
                        hid_device_connect(gamepad_config->host_mac_switch, &hid_cid);
                    }
                }
            }
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            // ssp: inform about user confirmation request
            printf("SSP User Confirmation Auto accept\n");
            break;
        case HCI_EVENT_HID_META:
            switch (hci_event_hid_meta_get_subevent_code(packet))
            {
            case HID_SUBEVENT_CONNECTION_OPENED:
                status = hid_subevent_connection_opened_get_status(packet);
                if (status)
                {
                    // outgoing connection failed
                    printf("Connection failed, status 0x%x\n", status);
                    gap_discoverable_control(1);
                    _connected = false;
                    hid_cid = 0;
                    return;
                }

                hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                bd_addr_t addr; 
                hid_subevent_connection_opened_get_bd_addr(packet, addr);

                bool comp = _compare_mac_addr(addr, gamepad_config->host_mac_switch);
                if(!comp)
                {
                    // New address, save 
                    memcpy(gamepad_config->host_mac_switch, addr, 6);
                    // hoja_set_notification_status(COLOR_GREEN);
                    settings_commit_blocks();
                }

                printf("HID Connected\n");
                _connected = true;
                hid_device_request_can_send_now_event(hid_cid);
                break;
            case HID_SUBEVENT_CONNECTION_CLOSED:
                printf("HID Disconnected\n");
                _connected = false;
                hid_cid = 0;
                break;
            case HID_SUBEVENT_CAN_SEND_NOW:
                if (hid_cid != 0)
                { 
                    uint32_t current_time_ms = btstack_run_loop_get_time_ms();
                    uint32_t time_elapsed = current_time_ms - last_hid_report_timestamp_ms;

                    if(time_elapsed >= BT_HAL_TARGET_POLLING_RATE_MS)
                    {
                        switch(hoja_get_status().gamepad_mode)
                        {
                            default:
                            swpro_hid_report(0, _bluetooth_hal_hid_tunnel);
                            break;

                            case GAMEPAD_MODE_SINPUT:
                            sinput_hid_report(sys_hal_time_us(), _bluetooth_hal_hid_tunnel);
                        }
                        
                        last_hid_report_timestamp_ms = current_time_ms;
                        hid_device_request_can_send_now_event(hid_cid);
                    }
                    else 
                    {
                        // Not enough time has passed, schedule another send event after delay
                        uint32_t time_elapsed = current_time_ms - last_hid_report_timestamp_ms;
                        uint32_t delay = BT_HAL_TARGET_POLLING_RATE_MS - time_elapsed;
                        btstack_run_loop_set_timer(&hid_timer, delay);
                        btstack_run_loop_set_timer_handler(&hid_timer, &hid_timer_handler);
                        btstack_run_loop_add_timer(&hid_timer);
                    }

                }
                break;
            case HID_SUBEVENT_SNIFF_SUBRATING_PARAMS:
                {
                    uint16_t max = hid_subevent_sniff_subrating_params_get_host_max_latency(packet);
                    uint16_t min = hid_subevent_sniff_subrating_params_get_host_min_timeout(packet);
                    printf("Sniff: %d, %d\n", max, min);
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

bool bluetooth_hal_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb)
{

    uint16_t this_vid = 0;
    uint16_t this_pid = 0;
    const char *this_name = NULL;
    const uint8_t *this_descriptor = NULL;
    uint16_t this_descriptor_size = 0;

    switch(device_mode)
    {
        default:
            this_name = "Pro Controller";
            this_vid = 0x057E;
            this_pid = 0x2009;
            this_descriptor = switch_bt_report_descriptor;
            this_descriptor_size = sizeof(switch_bt_report_descriptor);
        break;

        case GAMEPAD_MODE_SINPUT:
            this_name = device_static.name;
            this_vid = 0x2E8A;
            #if defined(HOJA_USB_PID)
            this_pid = HOJA_USB_PID, // board_config PID
            #else
            this_pid = 0x10C6, // Hoja Gamepad
            #endif
            this_descriptor = sinput_hid_report_descriptor;
            this_descriptor_size = sizeof(sinput_hid_report_descriptor);
        break;
    }

    // If the init fails it returns true lol
    if (cyw43_arch_init())
    {
        return false;
    }
    
    gap_set_bondable_mode(1);

    gap_set_class_of_device(0x2508);
    gap_set_local_name(this_name);

    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_ROLE_SWITCH |
                                         LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_set_allow_role_switch(true);

    hci_set_chipset(btstack_chipset_cyw43_instance());

    // L2CAP
    l2cap_init();
    
    sm_init();
    //sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    //sm_set_authentication_requirements(0);

    // SDP Server
    sdp_init();

    hid_sdp_record_t hid_sdp_record = {
        .hid_device_subclass    = 0x2508, // Device Subclass HID
        .hid_country_code       = 33,  // Country Code
        .hid_virtual_cable      = 1,   // HID Virtual Cable
        .hid_remote_wake        = 1,   // HID Remote Wake
        .hid_reconnect_initiate = 1,   // HID Reconnect Initiate
        .hid_normally_connectable = 0,   // HID Normally Connectable
        .hid_boot_device = 0,   // HID Boot Device
        .hid_ssr_host_max_latency = 0xFFFF,  // SSR Max Latency
        .hid_ssr_host_min_timeout = 0xFFFF,  // SSR Host Min Timeout
        .hid_supervision_timeout  = 3200,    // HID Supervision Timeout
        .hid_descriptor           = this_descriptor, // HID Descriptor
        .hid_descriptor_size      = this_descriptor_size, // HID Descriptor Length
        .device_name              = this_name
    }; // Device Name

    // Register SDP services

    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));
    hid_create_sdp_record(hid_service_buffer, sdp_create_service_record_handle(), &hid_sdp_record);
    //_create_sdp_hid_record(hid_service_buffer, &hid_sdp_record);
    sdp_register_service(hid_service_buffer);

    memset(pnp_service_buffer, 0, sizeof(pnp_service_buffer));

    device_id_create_sdp_record(pnp_service_buffer, sdp_create_service_record_handle(), DEVICE_ID_VENDOR_ID_SOURCE_USB, 
        this_vid, this_pid, 0x0100);
    //_create_sdp_pnp_record(pnp_service_buffer, 
    //    DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH, 0x057E, 0x2009, 0x0100);
    sdp_register_service(pnp_service_buffer);

    // HID Device
    hid_device_init(0, this_descriptor_size,
        this_descriptor);

    hci_event_callback_registration.callback = &_bt_hal_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hid_device_register_packet_handler(&_bt_hal_packet_handler);
    hid_device_register_report_data_callback(&_bt_hid_report_handler);

    _pairing_mode = pairing_mode;

    hci_power_control(HCI_POWER_ON);
    
    // btstack_run_loop_execute();

    return true;
}

void bluetooth_hal_stop()
{
}

void bluetooth_hal_task(uint64_t timestamp)
{   
    sleep_ms(2);
}

uint32_t bluetooth_hal_get_fwversion()
{
}

#endif