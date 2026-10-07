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
#include "hci_dump.h"

#include <string.h>
#include <stdarg.h>

#include "transport/transport_bt.h"
#include "utilities/settings.h"
#include "utilities/static_config.h"
#include "utilities/tasks.h"
#include "utilities/sysmon.h"
#include "devices/rgb.h"

#include "hal/sys_hal.h"
#include "transport/transport.h"

#include "hoja.h"
#include "cores/cores.h"
#include "utilities/crosscore_utils.h"

#include "nwii_lib_hid.h"

#define BT_HAL_TARGET_POLLING_RATE_MS 8
// A real Wii Remote reports every 10 ms
#define BT_HAL_WII_POLLING_RATE_MS 10
#define BT_HAL_INBOUND_FIFO_LEN 32

// A paired Wii Remote connects to the console, never the reverse, so keep paging the saved
// Wii until it answers (it may still be booting).
#define BT_HAL_WII_RECONNECT_MS 1000

// No report-stall watchdog: with several remotes connected the Wii can legitimately take seconds
// to service a link (sniff mode), and a watchdog then loops on reconnects. A silent link is left
// to the Wii's own supervision timeout, as with real remotes.

// Page timeout while reconnecting (x 0.625 ms, ~5 s) so retries during a reload stay short
#define BT_HAL_WII_PAGE_TIMEOUT 0x2000

// When a title starts the Wii reloads its system software and every link drops. Real remotes
// reconnect on their own, so after a lost link keep paging for this long before powering off.
#define BT_HAL_WII_LINK_LOST_WINDOW_MS 60000

volatile bool _connected = false;
volatile bool _hidreportclear = false;

volatile bool _pairing_mode = false;

static const char hid_device_name[] = "Wireless Gamepad";
static const char service_name[] = "Wireless Gamepad";
static uint8_t hid_service_buffer[700] = {0};
static uint8_t pnp_service_buffer[700] = {0};
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t hid_cid = 0;

static btstack_timer_source_t hid_report_timer;
static bool hid_report_timer_active = false;
static uint32_t hid_report_interval_ms = BT_HAL_TARGET_POLLING_RATE_MS;

static btstack_timer_source_t wii_reconnect_timer;
static bool wii_reconnect_timer_active = false;
static uint32_t wii_link_lost_deadline_ms = 0; // 0 while connected or before the first link
static hci_con_handle_t wii_acl_handle = HCI_CON_HANDLE_INVALID;

// Powering off, the Wii only closes the HID channels (as when quitting a title), so reconnecting
// starts as usual. A Wii in standby still accepts a new link but never answers the HID channel
// request (L2CAP RTX timeout), while a reloading Wii refuses it outright until it is ready. That
// tells them apart: on the standby answer, power off like a real remote instead of paging again.
static bool wii_fresh_acl = false; // This reconnect attempt brought up a new link

// Faster still: a Wii going to standby takes the page at the radio (it switches roles) and then
// never completes the connection, which otherwise ends only on a ~20 s timeout. While the Wii
// reloads for a title, a page always completes within a few seconds of the role switch.
#define BT_HAL_WII_PAGE_STALL_MS 5000

static btstack_timer_source_t wii_page_stall_timer;
static bool wii_page_pending = false; // Reconnect page in progress
static bool wii_page_answered = false; // ...and the Wii's radio has taken it

typedef struct
{
    uint8_t len;
    uint8_t data[64];
} bt_hal_inbound_report_s;

HOJA_CROSSCORE_FIFO_TYPE(bt_inbound, bt_hal_inbound_report_s, BT_HAL_INBOUND_FIFO_LEN);
static hoja_fifo_bt_inbound_t _bt_inbound_fifo;

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

static void _bt_hal_queue_inbound_report(const uint8_t *report, uint16_t len)
{
    if (!report || len == 0)
    {
        return;
    }

    bt_hal_inbound_report_s inbound = {0};
    if (len > sizeof(inbound.data))
    {
        len = sizeof(inbound.data);
    }

    memcpy(inbound.data, report, len);
    inbound.len = (uint8_t)len;
    (void)hoja_fifo_bt_inbound_push(&_bt_inbound_fifo, &inbound);
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
    _bt_hal_queue_inbound_report(_output_report_data, (uint16_t)(report_size + 1));
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

    _bt_hal_queue_inbound_report(report, (uint16_t)report_size);
}



static void _bt_hal_hid_report_timer_stop(void)
{
    if (!hid_report_timer_active)
    {
        return;
    }

    btstack_run_loop_remove_timer(&hid_report_timer);
    hid_report_timer_active = false;
}

static void _bt_hal_hid_report_timer_handler(btstack_timer_source_t *ts)
{
    if (!hid_cid || !_connected)
    {
        return;
    }

    hid_device_request_can_send_now_event(hid_cid);

    btstack_run_loop_set_timer(ts, hid_report_interval_ms);
    btstack_run_loop_add_timer(ts);
}

static void _bt_hal_hid_report_timer_start(void)
{
    _bt_hal_hid_report_timer_stop();

    btstack_run_loop_set_timer_handler(&hid_report_timer, &_bt_hal_hid_report_timer_handler);
    btstack_run_loop_set_timer(&hid_report_timer, hid_report_interval_ms);
    btstack_run_loop_add_timer(&hid_report_timer);
    hid_report_timer_active = true;
}

static bool _bt_hal_is_wii(void)
{
    return core_current_reportformat() == CORE_REPORTFORMAT_WII;
}

// The Wii's SYNC search only finds devices listening on the Limited Inquiry Access Code, so in
// Wii mode answer both it and the general code. Sent as soon as the controller can take a
// command, since BTstack has no GAP call for two IACs.
static bool _bt_hal_wii_iac_pending = true;

static void _bt_hal_wii_iac_task(void)
{
    if (!_bt_hal_wii_iac_pending || !_bt_hal_is_wii() || !hci_can_send_command_packet_now())
    {
        return;
    }

    _bt_hal_wii_iac_pending = false;
    hci_send_cmd(&hci_write_current_iac_lap_two_iacs, 2, NWII_HID_INQUIRY_ACCESS_CODE, GAP_IAC_GENERAL_INQUIRY);
}

static void _bt_hal_wii_reconnect_timer_stop(void)
{
    if (!wii_reconnect_timer_active)
    {
        return;
    }

    btstack_run_loop_remove_timer(&wii_reconnect_timer);
    wii_reconnect_timer_active = false;
}

static void _bt_hal_shutdown(void)
{
    tp_evt_s pevt = {
        .evt = TP_EVT_POWERCOMMAND,
        .evt_powercommand = {.power_command=TP_POWERCOMMAND_SHUTDOWN}
    };
    transport_evt_cb(pevt);
}

static void _bt_hal_wii_page_stall_handler(btstack_timer_source_t *ts)
{
    (void)ts;

    if (wii_page_pending && wii_page_answered && wii_link_lost_deadline_ms)
    {
        printf("Wii is going to standby, powering off\n");
        wii_link_lost_deadline_ms = 0;
        _bt_hal_wii_reconnect_timer_stop();
        _bt_hal_shutdown();
    }
    wii_page_pending = false;
}

static void _bt_hal_wii_reconnect_timer_handler(btstack_timer_source_t *ts)
{
    (void)ts;
    wii_reconnect_timer_active = false;

    if (hid_cid || _connected)
    {
        return;
    }

    if (wii_link_lost_deadline_ms &&
        (int32_t)(btstack_run_loop_get_time_ms() - wii_link_lost_deadline_ms) >= 0)
    {
        // The Wii never came back
        wii_link_lost_deadline_ms = 0;
        _bt_hal_shutdown();
        return;
    }

    wii_fresh_acl = false;
    if (wii_link_lost_deadline_ms)
    {
        wii_page_pending = true;
        wii_page_answered = false;
        btstack_run_loop_remove_timer(&wii_page_stall_timer);
        btstack_run_loop_set_timer_handler(&wii_page_stall_timer, &_bt_hal_wii_page_stall_handler);
        btstack_run_loop_set_timer(&wii_page_stall_timer, BT_HAL_WII_PAGE_STALL_MS);
        btstack_run_loop_add_timer(&wii_page_stall_timer);
    }
    hid_device_connect(gamepad_config->host_mac_wii, &hid_cid);
}

// When a title starts, the Wii reloads its system software but keeps the ACL link, and its new
// stack addresses L2CAP channels that no longer exist. BTstack answers with a Command Reject
// ("invalid CID") and otherwise drops the traffic, leaving the remote unregistered until the link
// times out. Watching for that reject (via BTstack's HCI packet-log hook, the only place it is
// visible) lets Wii mode drop the stale link and reconnect straight away, as a real remote does.
#define BT_HAL_L2CAP_COMMAND_REJECT     0x01
#define BT_HAL_L2CAP_REJECT_INVALID_CID 0x0002

// A reloading Wii keeps transmitting but stops acknowledging us, so a normal disconnect waits out
// the 30 s LMP response timeout. Power-cycling our radio drops the link locally in about a second
// (BTstack's halting watchdog discards connections the controller cannot close). The Wii is the
// master, so its own supervision timeout (up to 20 s) decides when it lets go of the old link;
// BTSTACK_EVENT_STATE powers the radio back on and keeps paging the Wii until it does.
static bool wii_radio_cycling = false;

static void _bt_hal_wii_radio_cycle(void)
{
    if (wii_radio_cycling)
        return;

    wii_radio_cycling = true;
    _bt_hal_hid_report_timer_stop();
    _connected = false;
    wii_link_lost_deadline_ms = btstack_run_loop_get_time_ms() + BT_HAL_WII_LINK_LOST_WINDOW_MS;
    hci_power_control(HCI_POWER_OFF);
}

// After the Wii closes the HID channels, give a normal disconnect this long before cycling
#define BT_HAL_WII_TEARDOWN_MS 1500

static btstack_timer_source_t wii_teardown_timer;
static hci_con_handle_t wii_teardown_handle = HCI_CON_HANDLE_INVALID;

static void _bt_hal_wii_teardown_handler(btstack_timer_source_t *ts)
{
    (void)ts;
    // Only if that same link is still up; a fresh reconnect may already have replaced it
    if (wii_teardown_handle != HCI_CON_HANDLE_INVALID && wii_teardown_handle == wii_acl_handle)
    {
        _bt_hal_wii_radio_cycle();
    }
    wii_teardown_handle = HCI_CON_HANDLE_INVALID;
}

// Quitting or launching a title can also leave the Wii holding the link without taking our
// reports, and nothing on the link reports it; a power cycle is the only way out by hand. If no
// report has gone out for this long, cycle the radio. With several remotes connected the Wii can
// take a second or two to service a link, so this stays well above that.
#define BT_HAL_WII_STALL_MS       6000
#define BT_HAL_WII_STALL_CHECK_MS 500

static btstack_timer_source_t wii_stall_timer;
static uint32_t wii_last_report_ms = 0;

static void _bt_hal_wii_stall_handler(btstack_timer_source_t *ts)
{
    if (_connected && hid_cid &&
        (btstack_run_loop_get_time_ms() - wii_last_report_ms) > BT_HAL_WII_STALL_MS)
    {
        _bt_hal_wii_radio_cycle();
    }

    btstack_run_loop_set_timer(ts, BT_HAL_WII_STALL_CHECK_MS);
    btstack_run_loop_add_timer(ts);
}

static btstack_timer_source_t wii_stale_link_timer;
static hci_con_handle_t wii_stale_link_handle = HCI_CON_HANDLE_INVALID;

static void _bt_hal_wii_stale_link_handler(btstack_timer_source_t *ts)
{
    (void)ts;
    if (wii_stale_link_handle != HCI_CON_HANDLE_INVALID)
    {
        wii_stale_link_handle = HCI_CON_HANDLE_INVALID;
        _bt_hal_wii_radio_cycle();
    }
}

static void _bt_hal_wii_dump_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len)
{
    // Outgoing ACL: handle(2) acl_len(2) l2cap_len(2) cid(2) code(1) id(1) len(2) reason(2)
    if (packet_type != HCI_ACL_DATA_PACKET || in || len < 14)
        return;
    if (little_endian_read_16(packet, 6) != L2CAP_CID_SIGNALING)
        return;
    if (packet[8] != BT_HAL_L2CAP_COMMAND_REJECT || little_endian_read_16(packet, 12) != BT_HAL_L2CAP_REJECT_INVALID_CID)
        return;
    if (wii_stale_link_handle != HCI_CON_HANDLE_INVALID)
        return;

    // Disconnect from a timer rather than from inside the HCI send path
    wii_stale_link_handle = little_endian_read_16(packet, 0) & 0x0FFF;
    btstack_run_loop_set_timer_handler(&wii_stale_link_timer, &_bt_hal_wii_stale_link_handler);
    btstack_run_loop_set_timer(&wii_stale_link_timer, 1);
    btstack_run_loop_add_timer(&wii_stale_link_timer);
}

static void _bt_hal_wii_dump_reset(void)
{
}

static void _bt_hal_wii_dump_log_message(int log_level, const char *format, va_list argptr)
{
    (void)log_level;
    (void)format;
    (void)argptr;
}

static const hci_dump_t _bt_hal_wii_stale_link_watch = {
    .reset       = _bt_hal_wii_dump_reset,
    .log_packet  = _bt_hal_wii_dump_log_packet,
    .log_message = _bt_hal_wii_dump_log_message,
};

static void _bt_hal_wii_reconnect_timer_start(void)
{
    _bt_hal_wii_reconnect_timer_stop();

    btstack_run_loop_set_timer_handler(&wii_reconnect_timer, &_bt_hal_wii_reconnect_timer_handler);
    btstack_run_loop_set_timer(&wii_reconnect_timer, BT_HAL_WII_RECONNECT_MS);
    btstack_run_loop_add_timer(&wii_reconnect_timer);
    wii_reconnect_timer_active = true;
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
            // Wii radio power-cycle (see _bt_hal_wii_radio_cycle): bring the radio straight back
            if (btstack_event_state_get_state(packet) == HCI_STATE_OFF && wii_radio_cycling)
            {
                wii_acl_handle = HCI_CON_HANDLE_INVALID;
                hid_cid = 0;
                _bt_hal_wii_iac_pending = true; // The controller forgets the IACs on reset
                hci_power_control(HCI_POWER_ON);
                // The custom address is only applied on the first power-on; without this the
                // radio returns with its factory address and the Wii refuses the unknown remote
                hci_set_bd_addr(core_current_params()->transport_dev_mac);
                return;
            }

            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
                return;

            wii_radio_cycling = false;
            _bt_hal_wii_iac_task();

            if(hid_cid) return;


            if (_pairing_mode)
            {
                gap_discoverable_control(1);
                return;
            }
            else
            {
                switch (core_current_reportformat())
                {
                default:
                case CORE_REPORTFORMAT_SWPRO:
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

                case CORE_REPORTFORMAT_SINPUT:
                    if (_bluetooth_hal_is_stored_identity_valid(gamepad_config->host_mac_sinput))
                    {
                        hid_device_connect(gamepad_config->host_mac_sinput, &hid_cid);
                    }
                    break;

                case CORE_REPORTFORMAT_WII:
                    // Stay discoverable either way: the Wii can still SYNC or temporarily
                    // connect to us while we page the saved console.
                    gap_discoverable_control(1);
                    if (_bluetooth_hal_is_stored_identity_valid(gamepad_config->host_mac_wii))
                    {
                        hid_device_connect(gamepad_config->host_mac_wii, &hid_cid);
                    }
                    break;
                }
            }
            break;

        case HCI_EVENT_ROLE_CHANGE:
            if (wii_page_pending && (hci_event_role_change_get_status(packet) == ERROR_CODE_SUCCESS))
            {
                wii_page_answered = true;
            }
            break;

        case HCI_EVENT_CONNECTION_COMPLETE:
            wii_page_pending = false;
            btstack_run_loop_remove_timer(&wii_page_stall_timer);
            if (hci_event_connection_complete_get_status(packet) == ERROR_CODE_SUCCESS)
            {
                wii_acl_handle = hci_event_connection_complete_get_connection_handle(packet);
                wii_fresh_acl = true;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (hci_event_disconnection_complete_get_connection_handle(packet) == wii_acl_handle)
            {
                wii_acl_handle = HCI_CON_HANDLE_INVALID;
            }
            if (hci_event_disconnection_complete_get_connection_handle(packet) == wii_teardown_handle)
            {
                btstack_run_loop_remove_timer(&wii_teardown_timer);
                wii_teardown_handle = HCI_CON_HANDLE_INVALID;
            }

            // A Wii that is powering off says so; follow it instead of trying to reconnect
            if (_bt_hal_is_wii() &&
                hci_event_disconnection_complete_get_reason(packet) ==
                    ERROR_CODE_REMOTE_DEVICE_TERMINATED_CONNECTION_DUE_TO_POWER_OFF)
            {
                _bt_hal_wii_reconnect_timer_stop();
                wii_link_lost_deadline_ms = 0;
                _bt_hal_shutdown();
            }
            break;

        case HCI_EVENT_COMMAND_COMPLETE:
        case HCI_EVENT_COMMAND_STATUS:
            // Retry the Wii IAC write if the controller was busy when BTstack came up
            if (hci_get_state() == HCI_STATE_WORKING)
            {
                _bt_hal_wii_iac_task();
            }
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            // Wii SYNC pairing is legacy PIN pairing; the PIN is the Wii's address reversed
            if (_bt_hal_is_wii())
            {
                bd_addr_t addr;
                uint8_t pin[NWII_HID_PIN_LEN];
                hci_event_pin_code_request_get_bd_addr(packet, addr);
                nwii_hid_make_pin(addr, pin);
                gap_pin_code_response_binary(addr, pin, NWII_HID_PIN_LEN);
            }
            break;

        case HCI_EVENT_LINK_KEY_NOTIFICATION:
            if(core_current_reportformat() == CORE_REPORTFORMAT_SWPRO)
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

                    // Reconnecting after a lost link, the Wii took a new link but never answered:
                    // it has gone to standby
                    if (_bt_hal_is_wii() && wii_link_lost_deadline_ms && wii_fresh_acl &&
                        (status == L2CAP_CONNECTION_RESPONSE_RESULT_RTX_TIMEOUT))
                    {
                        printf("Wii is in standby, powering off\n");
                        wii_link_lost_deadline_ms = 0;
                        _bt_hal_wii_reconnect_timer_stop();
                        _bt_hal_shutdown();
                        return;
                    }

                    if (_bt_hal_is_wii() && !_pairing_mode &&
                        _bluetooth_hal_is_stored_identity_valid(gamepad_config->host_mac_wii))
                    {
                        _bt_hal_wii_reconnect_timer_start();
                    }
                    return;
                }

                _bt_hal_wii_reconnect_timer_stop();

                hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                bd_addr_t addr;
                hid_subevent_connection_opened_get_bd_addr(packet, addr);

                uint8_t *addr_location = NULL;

                switch (core_current_reportformat())
                {
                default:
                case CORE_REPORTFORMAT_SWPRO:
                    addr_location = gamepad_config->host_mac_switch;
                    break;

                case CORE_REPORTFORMAT_SINPUT:
                    addr_location = gamepad_config->host_mac_sinput;
                    break;

                case CORE_REPORTFORMAT_WII:
                    addr_location = gamepad_config->host_mac_wii;
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
                if (_bt_hal_is_wii())
                {
                    // Paired or reconnected: from now on a dropped link pages this Wii again
                    _pairing_mode = false;
                }
                wii_link_lost_deadline_ms = 0;
                wii_last_report_ms = btstack_run_loop_get_time_ms();
                if (core_current_params()->core_connected)
                {
                    core_current_params()->core_connected();
                }
                _connected = true;
                _bt_hal_hid_report_timer_start();

                break;
            case HID_SUBEVENT_CONNECTION_CLOSED:
                printf("HID Disconnected\n");

                _bt_hal_hid_report_timer_stop();
                _connected = false;
                _hidreportclear = false;
                hid_cid = 0;

                // Torn down by our own radio power-cycle; BTSTACK_EVENT_STATE reconnects
                if (wii_radio_cycling)
                {
                    break;
                }

                if (_bt_hal_is_wii() && _bluetooth_hal_is_stored_identity_valid(gamepad_config->host_mac_wii))
                {
                    // Likely a title launch; reconnect unless the Wii says it is powering off
                    // (handled in HCI_EVENT_DISCONNECTION_COMPLETE). When the Wii only closed the
                    // HID channels (launching or quitting a title), its restarted stack will not
                    // answer on the old link, so drop it and page fresh; if the Wii does not
                    // acknowledge the disconnect, power-cycle the radio instead of waiting.
                    if (wii_acl_handle != HCI_CON_HANDLE_INVALID)
                    {
                        gap_disconnect(wii_acl_handle);
                        wii_teardown_handle = wii_acl_handle;
                        btstack_run_loop_remove_timer(&wii_teardown_timer);
                        btstack_run_loop_set_timer_handler(&wii_teardown_timer, &_bt_hal_wii_teardown_handler);
                        btstack_run_loop_set_timer(&wii_teardown_timer, BT_HAL_WII_TEARDOWN_MS);
                        btstack_run_loop_add_timer(&wii_teardown_timer);
                    }
                    wii_link_lost_deadline_ms = btstack_run_loop_get_time_ms() + BT_HAL_WII_LINK_LOST_WINDOW_MS;
                    _bt_hal_wii_reconnect_timer_start();
                    break;
                }

                _bt_hal_shutdown();
                break;
            case HID_SUBEVENT_CAN_SEND_NOW:
                wii_last_report_ms = btstack_run_loop_get_time_ms();
                if (hid_cid)
                {
                    core_report_s report = {0};

                    if (core_get_generated_report(&report))
                    {
                        _bluetooth_hal_hid_tunnel(report.data, report.size);
                    }
                }
                break;
            case HID_SUBEVENT_SNIFF_SUBRATING_PARAMS:
                rgb_set_idle(true);
                rgb_set_pulsing(COLOR_GREEN);
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

    while (cyw43_arch_init())
    {
        sys_hal_sleep_ms(1000);
    }

    _bt_init = true;

    const bool wii = (params->core_report_format == CORE_REPORTFORMAT_WII);
    hid_report_interval_ms = wii ? BT_HAL_WII_POLLING_RATE_MS : BT_HAL_TARGET_POLLING_RATE_MS;

    gap_set_bondable_mode(1);

    if (wii)
    {
        // The Wii only does legacy PIN pairing, and authenticates on its own terms; asking for
        // security ourselves breaks its temporary (no-pairing) connections.
        gap_ssp_set_enable(0);
        gap_set_security_level(LEVEL_0);
        gap_set_class_of_device(NWII_HID_CLASS_OF_DEVICE);
        hci_dump_init(&_bt_hal_wii_stale_link_watch);
        gap_set_link_supervision_timeout(NWII_HID_LINK_SUPERVISION_TIMEOUT);
        gap_set_page_timeout(BT_HAL_WII_PAGE_TIMEOUT);

        btstack_run_loop_set_timer_handler(&wii_stall_timer, &_bt_hal_wii_stall_handler);
        btstack_run_loop_set_timer(&wii_stall_timer, BT_HAL_WII_STALL_CHECK_MS);
        btstack_run_loop_add_timer(&wii_stall_timer);
    }
    else
    {
        gap_set_class_of_device(0x2508);
    }
    gap_set_local_name(_bt_hal_hid->name);

    // Every mode lets the host take the master role. A Wii in particular hangs up on a remote that
    // refuses the role switch once other remotes are connected or a game is running.
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
    if (wii)
    {
        // The Wii checks the HID record attributes, so serve a real remote's record verbatim.
        // It carries its own record handle, so it must be registered before any handle is
        // allocated for the PnP record below.
        const uint8_t *wii_record = NULL;
        uint16_t wii_record_len = 0;
        nwii_hid_get_sdp_record(&wii_record, &wii_record_len);
        memcpy(hid_service_buffer, wii_record, wii_record_len);
    }
    else
    {
        hid_create_sdp_record(hid_service_buffer, sdp_create_service_record_handle(), &hid_sdp_record);
    }
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

    _pairing_mode = (params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0;

    hci_power_control(HCI_POWER_ON);

    hci_set_bd_addr(_bt_hal_params->transport_dev_mac);

    // btstack_run_loop_execute();
    return true;
}

void transport_bt_task(uint64_t timestamp)
{
    (void)timestamp;

    bt_hal_inbound_report_s inbound;
    if (hoja_fifo_bt_inbound_pop(&_bt_inbound_fifo, &inbound))
    {
        core_report_tunnel_cb(inbound.data, inbound.len);
    }
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