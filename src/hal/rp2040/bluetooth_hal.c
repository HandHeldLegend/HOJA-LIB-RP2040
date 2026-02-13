#include "board_config.h"

#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_HAL)
#include "btstack_config.h"
#include "hal/bluetooth_hal.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_chipset_cyw43.h"
#include "btstack.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "btstack_tlv.h"

#include <string.h>

#include "switch/switch_commands.h"
#include "utilities/settings.h"
#include "utilities/static_config.h"
#include "utilities/interval.h"
#include "utilities/sysmon.h"
#include "devices/rgb.h"

#include "hal/sys_hal.h"
#include "transport/transport.h"

#include "hoja.h"

#define BT_HAL_TARGET_POLLING_RATE_MS 8

volatile uint32_t _bt_hal_pollrate_ms = BT_HAL_TARGET_POLLING_RATE_MS;

volatile bool _connected = false;
volatile bool _hidreportclear = false;

static const char hid_device_name[] = "Wireless Gamepad";
static const char service_name[] = "Wireless Gamepad";
static uint8_t hid_service_buffer[700] = {0};
static uint8_t pnp_service_buffer[700] = {0};
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

static inline void _bluetooth_hal_hid_tunnel(const void *report, uint16_t len)
{
    uint8_t new_report[66] = {0};
    new_report[0] = 0xA1; // Type of input report

    // Byte 1 is the report ID
    memcpy(&(new_report[1]), report, len);

    if (hid_cid)
    {
        hid_device_send_interrupt_message(hid_cid, new_report, len + 1);
    }
}

uint8_t _output_report_data[64] = {0};
const uint8_t *_output_report = &_output_report_data[0];

static void _bt_hid_report_handler(uint16_t cid,
                                   hid_report_type_t report_type,
                                   uint16_t report_id,
                                   int report_size, uint8_t *report)
{
    if (report_type != HID_REPORT_TYPE_OUTPUT) return;
    if (cid != hid_cid) return;
    if (!report || report_size == 0) return;

    printf("REPORT? %x, c: %d\n", report_id, hid_cid);

    if(report_id==0x01) hoja_set_notification_status(COLOR_RED);

    // 1. Set the Report ID as the first byte
    _output_report_data[0] = (uint8_t)report_id;

    if(report_size>63) report_size=63;

    // 2. Copy the actual report data starting at index 1
    // Use report_size, because 'report' points to the payload start
    memcpy(&_output_report_data[1], report, report_size);

    // 3. Total size is now (1 byte ID + report_size)
    core_report_tunnel_cb(_output_report, report_size + 1);
}

volatile bool _pairing_mode = false;
volatile bool _interval_reset = false;

// Timer handler to request can send now event after delay
static void hid_timer_handler(btstack_timer_source_t *ts)
{
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

            if (!hid_cid)
            {
                if (_pairing_mode)
                {
                    gap_delete_all_link_keys();
                    gap_discoverable_control(1);
                }
                else
                {
                    switch (hoja_get_status().gamepad_mode)
                    {
                    default:
                    case GAMEPAD_MODE_SWPRO:
                        if (gamepad_config->host_mac_switch[0] != 0xFF && gamepad_config->host_mac_switch[1] != 0xFF)
                        {
                            hid_device_connect(gamepad_config->host_mac_switch, &hid_cid);
                        }

                        break;

                    case GAMEPAD_MODE_SINPUT:
                        if (gamepad_config->host_mac_sinput[0] != 0xFF && gamepad_config->host_mac_sinput[1] != 0xFF)
                        {
                            hid_device_connect(gamepad_config->host_mac_sinput, &hid_cid);
                        }
                        break;
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

                uint8_t *addr_location = NULL;

                switch (hoja_get_status().gamepad_mode)
                {
                default:
                case GAMEPAD_MODE_SWPRO:
                    addr_location = gamepad_config->host_mac_switch;
                    break;

                case GAMEPAD_MODE_SINPUT:
                    addr_location = gamepad_config->host_mac_sinput;
                    break;
                }

                bool comp = _compare_mac_addr(addr, addr_location);
                if (!comp)
                {
                    // New address, save
                    memcpy(addr_location, addr, 6);
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
                _hidreportclear = false;
                hid_cid = 0;
                tp_evt_s pevt = {
                    .evt = TP_EVT_POWERCOMMAND,
                    .evt_powercommand = {.power_command=TP_POWERCOMMAND_SHUTDOWN}
                };
                transport_evt_cb(pevt);
                break;
            case HID_SUBEVENT_CAN_SEND_NOW:
                if (hid_cid)
                {
                    static uint64_t tmpstamp = 0;
                    sys_hal_time_us(&tmpstamp);

                    uint32_t current_time_ms = btstack_run_loop_get_time_ms();
                    uint32_t time_elapsed = current_time_ms - last_hid_report_timestamp_ms;

                    if (time_elapsed >= _bt_hal_pollrate_ms)
                    {
                        core_report_s report = {0};
                        
                        if(core_get_generated_report(&report))
                        {
                            _bluetooth_hal_hid_tunnel(report.data, report.size);
                        }
                        else break;

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
                uint16_t ms = (uint16_t)((max * 625) / 1000);
                _bt_hal_pollrate_ms = ms;
                if (!ms)
                    _bt_hal_pollrate_ms = 1;
                _interval_reset = true;
                uint16_t min = hid_subevent_sniff_subrating_params_get_host_min_timeout(packet);
                rgb_set_idle(true);
                hoja_set_notification_status(COLOR_GREEN);

                // printf("Sniff: %d, %d\n", max, min);
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

core_params_s *_bt_hal_params = NULL;
const core_hid_device_t *_bt_hal_hid = NULL;
volatile bool _bt_init = false;

// MODIFIED BTSTACK FUNCTION DEF
int hid_report_size_valid(uint16_t cid, int report_id, hid_report_type_t report_type, int report_size){
    if (!report_size) return 0;
    return 1;
}

/***********************************************/
/********* Transport Defines *******************/
void transport_bt_stop()
{
    //if(_bt_init)
    //    cyw43_arch_deinit();
    //_bt_init = false;
}

bool transport_bt_init(core_params_s *params)
{
    _bt_hal_params = params;

    if (!_bt_hal_params || !_bt_hal_params->hid_device)
        return false;

    _bt_hal_hid = _bt_hal_params->hid_device;

    if(params->core_pollrate_us < 8000)
    {
        _bt_hal_pollrate_ms = 8;
    }
    else
    {
        _bt_hal_pollrate_ms = params->core_pollrate_us/1000;
    }

    // If the init fails it returns true lol
    if (cyw43_arch_init())
    {
        return false;
    }
    
    _bt_init = true;

    gap_set_bondable_mode(1);

    gap_set_class_of_device(0x2508);
    gap_set_local_name(_bt_hal_hid->name);

    uint16_t link_policy = LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE;

    gap_set_default_link_policy_settings(link_policy);
    gap_set_allow_role_switch(true);

    hci_set_chipset(btstack_chipset_cyw43_instance());

    // L2CAP
    l2cap_init();

    sm_init();
    // sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    // sm_set_authentication_requirements(0);

    // SDP Server
    sdp_init();

    hid_sdp_record_t hid_sdp_record = {
        .hid_device_subclass = 0x2508,      // Device Subclass HID
        .hid_country_code = 33,             // Country Code
        .hid_virtual_cable = 1,             // HID Virtual Cable
        .hid_remote_wake = 1,               // HID Remote Wake
        .hid_reconnect_initiate = 1,        // HID Reconnect Initiate
        .hid_normally_connectable = 0,      // HID Normally Connectable
        .hid_boot_device = 0,               // HID Boot Device
        .hid_ssr_host_max_latency = 0xFFFF, // = x * 0.625ms
        .hid_ssr_host_min_timeout = 0xFFFF,
        .hid_supervision_timeout = 3200,             // HID Supervision Timeout
        .hid_descriptor         = _bt_hal_params->hid_device->hid_report_descriptor,           // HID Descriptor
        .hid_descriptor_size    = _bt_hal_params->hid_device->hid_report_descriptor_len,  // HID Descriptor Length
        .device_name = _bt_hal_hid->name};                   // Device Name

    // Register SDP services

    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));
    hid_create_sdp_record(hid_service_buffer, sdp_create_service_record_handle(), &hid_sdp_record);
    //_create_sdp_hid_record(hid_service_buffer, &hid_sdp_record);
    sdp_register_service(hid_service_buffer);

    memset(pnp_service_buffer, 0, sizeof(pnp_service_buffer));

    device_id_create_sdp_record(pnp_service_buffer, sdp_create_service_record_handle(), DEVICE_ID_VENDOR_ID_SOURCE_USB,
                                _bt_hal_hid->vid, _bt_hal_hid->pid, 0x0100);
    //_create_sdp_pnp_record(pnp_service_buffer,
    //    DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH, 0x057E, 0x2009, 0x0100);
    sdp_register_service(pnp_service_buffer);

    // HID Device
    hid_device_init(0, _bt_hal_hid->hid_report_descriptor_len,
                    _bt_hal_hid->hid_report_descriptor);

    hci_event_callback_registration.callback = &_bt_hal_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hid_device_register_packet_handler(&_bt_hal_packet_handler);
    hid_device_register_report_data_callback(&_bt_hid_report_handler);

    _pairing_mode = core_is_mac_blank(_bt_hal_params->transport_host_mac);

    hci_power_control(HCI_POWER_ON);

    hci_set_bd_addr(_bt_hal_params->transport_dev_mac);

    // btstack_run_loop_execute();
    return true;
}

void transport_bt_task(uint64_t timestamp)
{
}

uint32_t transport_bt_test()
{
    // If the init fails it returns true lol
    if (cyw43_arch_init())
    {
        return 0x00; // Return 0 for nothing
    }

    cyw43_arch_deinit();
    return 1; // PASS
}

#endif