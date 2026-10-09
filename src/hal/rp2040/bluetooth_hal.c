#include "board_config.h"

// BTstack Bluetooth for the RM2 (CYW43) and for an ESP32 running the HCI bridge
#if defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_BTSTACK
#include "btstack_config.h"
#include "hal/bluetooth_hal.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/multicore.h"
#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_HAL)
#include "pico/cyw43_arch.h"
#include "pico/btstack_chipset_cyw43.h"
#else
#include "drivers/bluetooth/esp32_hci.h"
#include "pico/async_context_threadsafe_background.h"
#include "pico/btstack_run_loop_async_context.h"
#include "pico/btstack_flash_bank.h"
#include "btstack_tlv_flash_bank.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "ble/le_device_db_tlv.h"
#include "btstack_memory.h"
#endif
#include "btstack.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "btstack_tlv.h"
#include "hci_dump.h"

#include <string.h>
#include <stdio.h>
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
#include "utilities/autodetect.h"

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
// reloads for a title, a page has completed within ~3.5 s of the role switch; leaving the Homebrew
// Channel can take longer, so allow 10 s.
#define BT_HAL_WII_PAGE_STALL_MS 10000

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

// SInput asks the host to poll every report interval (QoS, guaranteed service)
static hci_con_handle_t hid_con_handle = HCI_CON_HANDLE_INVALID;
static bool sinput_qos_pending = false;

static void _bt_hal_sinput_qos_task(void)
{
    if (!sinput_qos_pending || hid_con_handle == HCI_CON_HANDLE_INVALID || !hci_can_send_command_packet_now())
        return;
    sinput_qos_pending = false;

    uint32_t latency_us = hid_report_interval_ms * 1000;
    uint32_t token_rate = (64 * 1000) / hid_report_interval_ms;
    hci_send_cmd(&hci_qos_setup, hid_con_handle, 0, 0x02, token_rate, 0, latency_us, 0xFFFFFFFF);
}

#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HCI)
// A Switch report fills a 1-slot EDR packet exactly. On an active link (a PC) the ESP32 keeps
// falling back from those to smaller packets, splitting each report over several polls
// (~20 reports/s), so 1-slot EDR is left out and every report goes out in one 3-slot packet.
// In sniff (a Switch, 5 ms windows) only 1-slot packets fit, so every packet type stays allowed.
#define BT_HAL_ESP32_PACKET_TYPES_MULTISLOT 0xCC1E // DM1 DH1 DM3 DH3 DM5 DH5, no 2-DH1 or 3-DH1
#define BT_HAL_ESP32_PACKET_TYPES_ALL       0xCC18
static hci_con_handle_t packet_type_handle = HCI_CON_HANDLE_INVALID;
static uint16_t packet_type_pending = 0;
static uint16_t packet_type_current = BT_HAL_ESP32_PACKET_TYPES_ALL;
static uint8_t link_max_slots = 1;
static bool link_sniff = false;

static void _bt_hal_packet_type_update(hci_con_handle_t handle)
{
    if (handle != packet_type_handle)
    {
        // New link, controller defaults
        packet_type_handle = handle;
        packet_type_current = BT_HAL_ESP32_PACKET_TYPES_ALL;
        packet_type_pending = 0;
    }

    uint16_t types = (!link_sniff && link_max_slots >= 3) ? BT_HAL_ESP32_PACKET_TYPES_MULTISLOT : BT_HAL_ESP32_PACKET_TYPES_ALL;
    if (types != packet_type_current)
    {
        packet_type_current = types;
        packet_type_pending = types;
    }
}

static void _bt_hal_packet_type_task(void)
{
    if (!packet_type_pending || packet_type_handle == HCI_CON_HANDLE_INVALID || !hci_can_send_command_packet_now())
        return;

    hci_send_cmd(&hci_change_connection_packet_type, packet_type_handle, packet_type_pending);
    packet_type_pending = 0;
}
#endif


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

// Schedule against fixed deadlines so tick latency doesn't stretch the interval
static uint32_t hid_report_deadline_ms = 0;

static void _bt_hal_hid_report_timer_arm(btstack_timer_source_t *ts)
{
    uint32_t now = btstack_run_loop_get_time_ms();
    hid_report_deadline_ms += hid_report_interval_ms;

    int32_t wait = (int32_t)(hid_report_deadline_ms - now);
    if (wait < 0)
    {
        // A whole interval behind, restart from now instead of bursting
        hid_report_deadline_ms = now + hid_report_interval_ms;
        wait = hid_report_interval_ms;
    }

    btstack_run_loop_set_timer(ts, wait);
    btstack_run_loop_add_timer(ts);
}

static void _bt_hal_hid_report_timer_handler(btstack_timer_source_t *ts)
{
    if (!hid_cid || !_connected)
    {
        return;
    }

    hid_device_request_can_send_now_event(hid_cid);
    _bt_hal_sinput_qos_task();
#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HCI)
    _bt_hal_packet_type_task();
#endif

    _bt_hal_hid_report_timer_arm(ts);
}

static void _bt_hal_hid_report_timer_start(void)
{
    _bt_hal_hid_report_timer_stop();

    btstack_run_loop_set_timer_handler(&hid_report_timer, &_bt_hal_hid_report_timer_handler);
    hid_report_deadline_ms = btstack_run_loop_get_time_ms();
    _bt_hal_hid_report_timer_arm(&hid_report_timer);
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

// Called from Bluetooth callbacks: the shutdown itself runs from transport_bt_task
static volatile bool _bt_hal_shutdown_pending = false;

static void _bt_hal_shutdown(void)
{
    _bt_hal_shutdown_pending = true;
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

// Reconnect to the host saved for this mode
static void _bt_hal_reconnect_saved_host(void)
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

// The stack comes up once per boot. Auto mode may set up a mode after it is running.
static bool _bt_stack_up = false;
static volatile bool _bt_power_cycling = false;
static bool _bt_mode_ready = false;

// The address the radio runs with, and the one it comes back with after a power cycle
static bd_addr_t _bt_radio_mac;
static bd_addr_t _bt_cycle_mac;

static void _bt_hal_power_on_as(const uint8_t *mac)
{
    memcpy(_bt_radio_mac, mac, 6);
#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HCI)
    esp32_hci_set_radio_mac(mac);
#endif
    hci_power_control(HCI_POWER_ON);
    hci_set_bd_addr(_bt_radio_mac);
}

// Auto mode: before reconnecting, call each saved host and open a service discovery channel (no
// pairing, no HID). An awake host answers, even if only to refuse. A Wii in standby takes the call
// but never answers. The first awake host picks the mode, Switch, then Wii, then SInput.
// Hosts listen for calls every 1.28 s, so each gets two of those windows
#define BT_HAL_PROBE_PAGE_TIMEOUT   0x1000 // x 0.625 ms, 2.56 s per host
#define BT_HAL_PROBE_ANSWER_MS      1500
#define BT_HAL_DEFAULT_PAGE_TIMEOUT 0x6000 // BTstack's default

typedef struct
{
    core_reportformat_t format;
    const uint8_t *addr;
    bd_addr_t local_mac; // Our address in that mode: hosts only answer the remote they paired
} bt_hal_probe_host_s;

static bt_hal_probe_host_s probe_hosts[3];
static uint8_t probe_count = 0;
static uint8_t probe_index = 0;
static bool probe_running = false;
static uint16_t probe_cid = 0;
static hci_con_handle_t probe_handle = HCI_CON_HANDLE_INVALID;
static btstack_timer_source_t probe_timer;

static void _bt_hal_probe_next(void);
static void _bt_hal_mode_setup(core_params_s *params);
static void _bt_hal_power_cycle(void);
static void _bt_hal_power_cycle_as(const uint8_t *mac);

static void _bt_hal_probe_add(core_reportformat_t format, const uint8_t *addr)
{
    if (!_bluetooth_hal_is_stored_identity_valid(addr))
        return;

    // A host saved for two modes answers for both: the first mode keeps it
    for (uint8_t i = 0; i < probe_count; i++)
    {
        if (!memcmp(addr, probe_hosts[i].addr, 6))
            return;
    }

    probe_hosts[probe_count].format = format;
    probe_hosts[probe_count].addr = addr;
    transport_mode_mac(probe_hosts[probe_count].local_mac, format);
    probe_count++;
}

static void _bt_hal_probe_finish(core_reportformat_t found)
{
    probe_running = false;
    gap_set_page_timeout(BT_HAL_DEFAULT_PAGE_TIMEOUT);

    // Switch mode carries on: set it up now and bring the radio back with it. Otherwise core 1
    // brings up the mode that answered.
    if (autodetect_bt_probe_result(found))
    {
        _bt_hal_mode_setup(core_current_params());
        _bt_hal_power_cycle();
    }
}

static void _bt_hal_probe_host_done(bool awake)
{
    btstack_run_loop_remove_timer(&probe_timer);
    if (probe_cid)
    {
        l2cap_disconnect(probe_cid);
        probe_cid = 0;
    }

    if (awake)
    {
        _bt_hal_probe_finish(probe_hosts[probe_index].format);
        return;
    }

    if (probe_handle != HCI_CON_HANDLE_INVALID)
        gap_disconnect(probe_handle);
    probe_handle = HCI_CON_HANDLE_INVALID;

    probe_index++;
    _bt_hal_probe_next();
}

// Connected but no answer: asleep
static void _bt_hal_probe_timeout(btstack_timer_source_t *ts)
{
    (void)ts;
    if (probe_running)
        _bt_hal_probe_host_done(false);
}

static void _bt_hal_probe_l2cap_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (!probe_running || packet_type != HCI_EVENT_PACKET || packet[0] != L2CAP_EVENT_CHANNEL_OPENED)
        return;

    const uint8_t status = l2cap_event_channel_opened_get_status(packet);
    if (status != ERROR_CODE_SUCCESS)
        probe_cid = 0;

    // Opened, or refused by the host itself: awake. A failed call or no answer: not
    const bool answered = (status == ERROR_CODE_SUCCESS) ||
                          (probe_handle != HCI_CON_HANDLE_INVALID &&
                           status >= L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM &&
                           status <= L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES);
    _bt_hal_probe_host_done(answered);
}

// From HCI_EVENT_CONNECTION_COMPLETE: the call went through, now wait for the answer
static void _bt_hal_probe_on_connection(const uint8_t *packet)
{
    if (!probe_running || hci_event_connection_complete_get_status(packet) != ERROR_CODE_SUCCESS)
        return;

    bd_addr_t addr;
    hci_event_connection_complete_get_bd_addr(packet, addr);
    if (memcmp(addr, probe_hosts[probe_index].addr, 6))
        return;

    probe_handle = hci_event_connection_complete_get_connection_handle(packet);
    btstack_run_loop_remove_timer(&probe_timer);
    btstack_run_loop_set_timer_handler(&probe_timer, &_bt_hal_probe_timeout);
    btstack_run_loop_set_timer(&probe_timer, BT_HAL_PROBE_ANSWER_MS);
    btstack_run_loop_add_timer(&probe_timer);
}

static void _bt_hal_probe_next(void)
{
    if (probe_index >= probe_count)
    {
        _bt_hal_probe_finish(CORE_REPORTFORMAT_UNDEFINED);
        return;
    }

    // Call each host from the address it paired with. The probe carries on once the radio is back.
    if (memcmp(_bt_radio_mac, probe_hosts[probe_index].local_mac, 6))
    {
        _bt_hal_power_cycle_as(probe_hosts[probe_index].local_mac);
        return;
    }

    bd_addr_t addr;
    memcpy(addr, probe_hosts[probe_index].addr, 6);
    probe_handle = HCI_CON_HANDLE_INVALID;
    if (l2cap_create_channel(&_bt_hal_probe_l2cap_handler, addr, BLUETOOTH_PSM_SDP, 48, &probe_cid) != ERROR_CODE_SUCCESS)
    {
        probe_cid = 0;
        _bt_hal_probe_host_done(false);
    }
}

// True while the probe runs, the reconnect follows from its result
static bool _bt_hal_probe_start(void)
{
    // Back from changing address for the next host
    if (probe_running)
    {
        _bt_hal_probe_next();
        return true;
    }
    if (!autodetect_bt_probe_pending())
        return false;

    probe_count = 0;
    probe_index = 0;
    _bt_hal_probe_add(CORE_REPORTFORMAT_SWPRO, gamepad_config->host_mac_switch);
    _bt_hal_probe_add(CORE_REPORTFORMAT_WII, gamepad_config->host_mac_wii);
    _bt_hal_probe_add(CORE_REPORTFORMAT_SINPUT, gamepad_config->host_mac_sinput);

    if (!probe_count)
    {
        _bt_hal_probe_finish(CORE_REPORTFORMAT_UNDEFINED);
        return true;
    }

    probe_running = true;
    gap_set_page_timeout(BT_HAL_PROBE_PAGE_TIMEOUT);
    _bt_hal_probe_next();
    return true;
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
                // The custom address is only applied on the first power-on; without this the
                // radio returns with its factory address and the Wii refuses the unknown remote
                _bt_hal_power_on_as(core_current_params()->transport_dev_mac);
                return;
            }

            // A new mode, or the probe calling the next host, comes back with its own address
            if (btstack_event_state_get_state(packet) == HCI_STATE_OFF && _bt_power_cycling)
            {
                _bt_power_cycling = false;
                _bt_hal_power_on_as(_bt_cycle_mac);
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
            else if (!_bt_mode_ready && !autodetect_bt_probe_pending())
            {
                // The probe gave up before deciding: set up this mode now
                _bt_hal_mode_setup(core_current_params());
                _bt_hal_power_cycle();
            }
            else if (!_bt_hal_probe_start())
            {
                _bt_hal_reconnect_saved_host();
            }
            break;


#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HCI)
        case HCI_EVENT_MAX_SLOTS_CHANGED:
        {
            hci_con_handle_t handle = hci_event_max_slots_changed_get_handle(packet);
            if (handle != packet_type_handle)
                link_sniff = false;
            link_max_slots = hci_event_max_slots_changed_get_lmp_max_slots(packet);
            _bt_hal_packet_type_update(handle);
        }
        break;

        case HCI_EVENT_MODE_CHANGE:
        {
            hci_con_handle_t handle = hci_event_mode_change_get_handle(packet);
            if (handle != packet_type_handle)
                link_max_slots = 1;
            link_sniff = hci_event_mode_change_get_mode(packet) != 0; // Not active
            _bt_hal_packet_type_update(handle);
        }
        break;
#endif

        case HCI_EVENT_ROLE_CHANGE:
            if (wii_page_pending && (hci_event_role_change_get_status(packet) == ERROR_CODE_SUCCESS))
            {
                wii_page_answered = true;
            }
            break;

        case HCI_EVENT_CONNECTION_COMPLETE:
            _bt_hal_probe_on_connection(packet);
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
                hid_con_handle = hid_subevent_connection_opened_get_con_handle(packet);
                sinput_qos_pending = (core_current_reportformat() == CORE_REPORTFORMAT_SINPUT);
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
                // A connected controller stops answering searches, like a real one. Otherwise a
                // console searching for another controller finds this one again (on a Wii, pairing
                // another remote stalled until it restarted, or it locked up).
                gap_discoverable_control(0);

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
                if (wii_radio_cycling || _bt_power_cycling)
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
                    // Findable again while reconnecting, so the Wii can SYNC or connect to us
                    gap_discoverable_control(1);
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
/********* Controller bring-up *****************/

#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_HAL)

static bool _bt_hal_controller_init(core_params_s *params)
{
    (void)params;
    // Sets up BTstack, its run loop, transport and link key storage
    while (cyw43_arch_init())
    {
        sys_hal_sleep_ms(1000);
    }
    hci_set_chipset(btstack_chipset_cyw43_instance());
    return true;
}

static bool _bt_hal_update_mode(void)
{
    return false;
}

// BTstack runs in the async context: hold its lock to call into it from core 1
static void _bt_hal_lock(void)
{
    async_context_acquire_lock_blocking(cyw43_arch_async_context());
}

static void _bt_hal_unlock(void)
{
    async_context_release_lock(cyw43_arch_async_context());
}

#else

static async_context_threadsafe_background_t _bt_async;
static bool _bt_hal_legacy = false; // ESP32 still on the old baseband

// Link keys in the BTstack flash bank, as btstack_cyw43_init does
static void _bt_hal_setup_tlv(void)
{
    static btstack_tlv_flash_bank_t tlv_context;
    const btstack_tlv_t *tlv = btstack_tlv_flash_bank_init_instance(&tlv_context, pico_flash_bank_instance(), NULL);
    btstack_tlv_set_instance(tlv, &tlv_context);
    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(tlv, &tlv_context));
#ifdef ENABLE_BLE
    le_device_db_tlv_configure(tlv, &tlv_context);
#endif
}

static bool _bt_hal_controller_init(core_params_s *params)
{
    if (!esp32_hci_backend_init(params))
        return false;
    if (esp32_hci_backend_update_mode())
        return true;

    async_context_threadsafe_background_config_t config = async_context_threadsafe_background_default_config();
    if (!async_context_threadsafe_background_init(&_bt_async, &config))
        return false;

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_async_context_get_instance(&_bt_async.core));
    hci_init(esp32_hci_transport_instance(), NULL);
    _bt_hal_setup_tlv();
    return true;
}

static bool _bt_hal_update_mode(void)
{
    return esp32_hci_backend_update_mode();
}

// BTstack runs in the async context: hold its lock to call into it from core 1
static void _bt_hal_lock(void)
{
    async_context_acquire_lock_blocking(&_bt_async.core);
}

static void _bt_hal_unlock(void)
{
    async_context_release_lock(&_bt_async.core);
}

#endif

// Everything that belongs to one mode: class of device, security, SDP records and the HID service
static void _bt_hal_mode_setup(core_params_s *params)
{
    const bool wii = (params->core_report_format == CORE_REPORTFORMAT_WII);
    const bool sinput = (params->core_report_format == CORE_REPORTFORMAT_SINPUT);
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
    // Sniff on a PC (12.5 ms on Windows) caps SInput below its report rate
    if (sinput)
        link_policy = LM_LINK_POLICY_ENABLE_ROLE_SWITCH;

    gap_set_default_link_policy_settings(link_policy);
    gap_set_allow_role_switch(true);

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

    hid_device_register_packet_handler(&_bt_hal_packet_handler);
    hid_device_register_report_data_callback(&_bt_hid_report_handler);
    hid_device_register_set_report_callback(&_bt_hid_set_report_handler);

    _pairing_mode = (params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0;
    _bt_mode_ready = true;
}

static void _bt_hal_power_cycle_as(const uint8_t *mac)
{
    memcpy(_bt_cycle_mac, mac, 6);
    _bt_power_cycling = true;
    hid_cid = 0;
    _connected = false;
    hci_power_control(HCI_POWER_OFF);
}

// Brings the radio back with the current mode's address
static void _bt_hal_power_cycle(void)
{
    _bt_hal_power_cycle_as(core_current_params()->transport_dev_mac);
}

/***********************************************/
/********* Transport Defines *******************/
void transport_bt_stop()
{
#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HCI)
    if (_bt_hal_legacy)
    {
        esp32_legacy_bt_stop();
        return;
    }
#endif
    //if(_bt_init)
    //    cyw43_arch_deinit();
    //_bt_init = false;
}

bool transport_bt_init(core_params_s *params)
{
    _bt_hal_params = params;

    if (!_bt_hal_params)
        return false;

#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HCI)
    // ESP32 firmware update (no core or HID device)
    if (params->core_boot_flags & COREBOOT_FLAG_ALTFLASH)
        return esp32_hci_backend_init(params);

    // Old baseband: its own driver until the ESP32 is updated
    uint16_t esp32_version = esp32_hci_firmware_version();
    if (esp32_version && esp32_version < ESP32_HCI_BRIDGE_VERSION_MIN)
    {
        _bt_hal_legacy = true;
        bool ok = esp32_legacy_bt_init(params);
        esp32_hci_console_attach();
        return ok;
    }
#endif

    if (!_bt_hal_params->hid_device)
        return false;

    _bt_hal_hid = _bt_hal_params->hid_device;

    if (!_bt_stack_up)
    {
        if (!_bt_hal_controller_init(params))
            return false;

        if (_bt_hal_update_mode())
            return true;

        _bt_init = true;

        // L2CAP
        l2cap_init();

        sm_init();
        // sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
        // sm_set_authentication_requirements(0);

        // SDP Server
        sdp_init();

        hci_event_callback_registration.callback = &_bt_hal_packet_handler;
        hci_add_event_handler(&hci_event_callback_registration);
        _bt_stack_up = true;

        // Auto mode checks the saved hosts before any mode is set up
        if (!autodetect_bt_probe_pending())
            _bt_hal_mode_setup(params);

        _bt_hal_power_on_as(_bt_hal_params->transport_dev_mac);
        return true;
    }

    // Already running (Auto mode picked this mode): set it up and bring the radio back with it
    _bt_hal_lock();
    _bt_hal_mode_setup(params);
    _bt_hal_power_cycle();
    _bt_hal_unlock();
    return true;
}

void transport_bt_task(uint64_t timestamp)
{
    (void)timestamp;
#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HCI)
    if (_bt_hal_legacy)
    {
        esp32_legacy_bt_task(timestamp);
        return;
    }
    esp32_hci_backend_task(timestamp);
    if (_bt_hal_update_mode())
        return;

    // The ESP32 lost its links behind the stack's back: restart the stack, which closes them
    // properly and reconnects. Once a restart is under way, leave it be: each power-off call
    // cancels the stack's timer that finishes shutting down without the controller.
    if (esp32_hci_take_radio_fault() && !_bt_power_cycling && !wii_radio_cycling)
    {
        _bt_hal_lock();
        _bt_hal_power_cycle();
        _bt_hal_unlock();
    }
#endif

    // A shutdown asked for from a Bluetooth callback runs here, outside the stack
    if (_bt_hal_shutdown_pending)
    {
        _bt_hal_shutdown_pending = false;
        tp_evt_s pevt = {
            .evt = TP_EVT_POWERCOMMAND,
            .evt_powercommand = {.power_command = TP_POWERCOMMAND_SHUTDOWN}
        };
        transport_evt_cb(pevt);
        return;
    }

    bt_hal_inbound_report_s inbound;
    if (hoja_fifo_bt_inbound_pop(&_bt_inbound_fifo, &inbound))
    {
        core_report_tunnel_cb(inbound.data, inbound.len);
    }
}

#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_HAL)
// The ESP32 backend provides its own static info (esp32_hci.c)
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

#endif