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

#include "transport/transport_bt.h"
#include "utilities/settings.h"
#include "utilities/static_config.h"
#include "utilities/tasks.h"
#include "utilities/sysmon.h"
#include "devices/rgb.h"

#include "hal/sys_hal.h"
#include "transport/transport.h"

#include "hoja.h"

#define BT_HAL_TARGET_POLLING_RATE_MS 8

volatile uint32_t _bt_hal_pollrate_ms = BT_HAL_TARGET_POLLING_RATE_MS;

volatile bool _connected = false;
volatile bool _hidreportclear = false;

volatile bool _pairing_mode = false;
volatile bool _interval_reset = false;

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

/** True when persisted pairing bytes are not blank (0x0000) or erased (0xFFFF…) sentinel. */
static bool _bluetooth_hal_is_stored_identity_valid(const uint8_t *bytes)
{
    if (bytes[0] == 0xFF && bytes[1] == 0xFF)
    {
        return false;
    }
    if (!bytes[0] && !bytes[1])
    {
        return false;
    }
    return true;
}

static inline void _bluetooth_hal_reverse_bytes(const uint8_t *in, uint8_t *out, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        out[i] = in[len - 1 - i];
    }
}

// Compare addresses and return true if they are the same
bool _bluetooth_hal_is_mac_addr_same(const bd_addr_t addr1, const bd_addr_t addr2)
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

bool _bluetooth_hal_is_lk_addr_same(uint8_t lk1[16], uint8_t lk2[16])
{
    for (int i = 0; i < 16; i++)
    {
        if (lk1[i] != lk2[i])
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
        tasks_mark_sent_isr();
    }
}

uint8_t _output_report_data[64] = {0};
const uint8_t *_output_report = &_output_report_data[0];

static void _bt_hid_output_tunnel(const uint8_t *report, uint16_t len)
{
    if (!report || len == 0)
    {
        return;
    }
    if (len > sizeof(_output_report_data))
    {
        len = sizeof(_output_report_data);
    }
    core_report_tunnel_cb(report, len);
}

/** Interrupt-channel DATA output (report id separate from payload). */
static void _bt_hid_report_handler(uint16_t cid,
                                   hid_report_type_t report_type,
                                   uint16_t report_id,
                                   int report_size, uint8_t *report)
{
    if (report_type != HID_REPORT_TYPE_OUTPUT)
    {
        return;
    }
    if (cid != hid_cid)
    {
        return;
    }
    if (!report || report_size == 0)
    {
        return;
    }

    _output_report_data[0] = (uint8_t)report_id;

    if (report_size > 63)
    {
        report_size = 63;
    }

    memcpy(&_output_report_data[1], report, (size_t)report_size);
    _bt_hid_output_tunnel(_output_report, (uint16_t)(report_size + 1));
}

/** Control-channel SET_REPORT output (report id is report[0]). Used by Switch. */
static void _bt_hid_set_report_handler(uint16_t cid,
                                       hid_report_type_t report_type,
                                       int report_size,
                                       uint8_t *report)
{
    if (report_type != HID_REPORT_TYPE_OUTPUT)
    {
        return;
    }
    if (cid != hid_cid)
    {
        return;
    }
    if (!report || report_size <= 0)
    {
        return;
    }

    _bt_hid_output_tunnel(report, (uint16_t)report_size);
}



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

            if(hid_cid) return;


            if (_pairing_mode)
            {
                gap_discoverable_control(1);
                return;
            }
            else
            {
                switch (hoja_get_status().gamepad_mode)
                {
                default:
                case GAMEPAD_MODE_SWPRO:
                    link_key_type_t read_type;
                    link_key_t read_key;

                    bool overwrite_key = false;

                    if (!_bluetooth_hal_is_stored_identity_valid(switchpair_config->link_key))
                    {
                        gap_discoverable_control(1);
                        return;
                    }

                    if (gap_get_link_key_for_bd_addr(gamepad_config->host_mac_switch, read_key, &read_type))
                    {
                        //printf("BTStack Stored Link Key:\n");
                        link_key_t read_key_be;
                        _bluetooth_hal_reverse_bytes(read_key, read_key_be, 16);

                        if (!_bluetooth_hal_is_lk_addr_same(read_key_be, switchpair_config->link_key))
                        {
                            overwrite_key = true;
                        }
                    }
                    else
                    {
                        overwrite_key = true;
                    }

                    if(overwrite_key)
                    {
                        link_key_t link_key_le;
                        _bluetooth_hal_reverse_bytes(switchpair_config->link_key, link_key_le, 16);
                        gap_store_link_key_for_bd_addr(gamepad_config->host_mac_switch, link_key_le,
                                          UNAUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P192);
                    }

                    hid_device_connect(gamepad_config->host_mac_switch, &hid_cid);

                    break;

                case GAMEPAD_MODE_SINPUT:
                    if (_bluetooth_hal_is_stored_identity_valid(gamepad_config->host_mac_sinput))
                    {
                        hid_device_connect(gamepad_config->host_mac_sinput, &hid_cid);
                    }
                    break;
                }
            }
            break;

        case HCI_EVENT_LINK_KEY_NOTIFICATION:
            if(hoja_get_status().gamepad_mode == GAMEPAD_MODE_SWPRO)
            {
                bd_addr_t addr;
                hci_event_link_key_request_get_bd_addr(packet, addr);

                link_key_t link_key_be;
                link_key_t link_key_le;

                /* BTstack reports link keys in little-endian format for legacy reasons. */
                memcpy(link_key_le, &packet[8], 16);

                /* Store a big-endian copy so the flash contents and debug logs stay human-readable. */
                _bluetooth_hal_reverse_bytes(link_key_le, link_key_be, 16);

                ns_usbpair_s usbpair = {0};
                memcpy(usbpair.host_mac, addr, 6);
                memcpy(usbpair.link_key, link_key_be, 16);
                ns_api_hook_set_usbpair(usbpair);
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

                bool comp = _bluetooth_hal_is_mac_addr_same(addr, addr_location);
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
                        
                        if (core_get_generated_report(&report))
                        {
                            _bluetooth_hal_hid_tunnel(report.data, report.size);
                            last_hid_report_timestamp_ms = current_time_ms;
                        }

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

    while (cyw43_arch_init())
    {
        sys_hal_sleep_ms(1000);
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

    hid_device_accept_truncated_hid_reports(true);

    hci_event_callback_registration.callback = &_bt_hal_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hid_device_register_packet_handler(&_bt_hal_packet_handler);
    hid_device_register_report_data_callback(&_bt_hid_report_handler);
    hid_device_register_set_report_callback(&_bt_hid_set_report_handler);

    _pairing_mode = params->core_boot_flags & COREBOOT_FLAG_PAIR;

    hci_power_control(HCI_POWER_ON);

    hci_set_bd_addr(_bt_hal_params->transport_dev_mac);

    // btstack_run_loop_execute();
    return true;
}

void transport_bt_task(uint64_t timestamp)
{
    (void)timestamp;
    sleep_us(250);
}

static uint32_t _bt_hal_probe_wireless(void)
{
    // If the init fails it returns true lol
    if (cyw43_arch_init())
    {
        return 0x00;
    }

    cyw43_arch_deinit();
    return 1;
}

void transport_bt_static_get_caps(transport_bt_static_caps_s *caps)
{
    if (caps == NULL)
    {
        return;
    }

    caps->bdr_supported = 1;
    caps->ble_supported = 0;
    caps->external_update_supported = 0;
}

uint8_t transport_bt_static_part_status(void)
{
    return _bt_hal_probe_wireless() > 0u ? TRANSPORT_WIRELESS_PART_OK : TRANSPORT_WIRELESS_PART_ERROR;
}

uint16_t transport_bt_static_external_version(void)
{
    return 0;
}

const char *bluetooth_driver_part_code(void)
{
    return "RPI RM2";
}

#endif