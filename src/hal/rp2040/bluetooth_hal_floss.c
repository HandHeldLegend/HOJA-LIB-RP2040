#include "board_config.h"

// FLOSS Bluetooth for the RM2 (CYW43). Same behaviour as the BTstack backend (bluetooth_hal.c):
// Switch, SInput and Wii modes, Auto mode's host probe, the Wii's reload and standby handling.
#if defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_FLOSS
#include "hal/bluetooth_hal.h"
#include "hal/rp2040/bt_flash_bank.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/flash.h"
#include "pico/cyw43_arch.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "floss/floss.h"
#include "floss_cyw43.h"

#include <string.h>
#include <stdio.h>

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

// Test builds: report timing diagnostics in SInput reports (see _bt_hal_diag_fill)
#ifndef HOJA_BT_FLOSS_DIAG
#define HOJA_BT_FLOSS_DIAG 0
#endif

/***********************************************/
/********* FLOSS platform (HAL) ****************/

uint32_t floss_hal_time_us(void)
{
    return time_us_32();
}

// floss_hid_set_input() is the only call that may come from elsewhere; a spin lock covers both cores
static spin_lock_t *_floss_lock = NULL;

uint32_t floss_hal_critical_enter(void)
{
    if (_floss_lock)
        return spin_lock_blocking(_floss_lock);
    return save_and_disable_interrupts();
}

void floss_hal_critical_exit(uint32_t state)
{
    if (_floss_lock)
        spin_unlock(_floss_lock, state);
    else
        restore_interrupts(state);
}

void floss_hal_random(uint8_t *buf, size_t len)
{
    while (len)
    {
        uint32_t r = get_rand_32();
        size_t n = len < 4 ? len : 4;
        memcpy(buf, &r, n);
        buf += n;
        len -= n;
    }
}

void floss_hal_log_write(const char *text, size_t len)
{
#if HOJA_BT_LOGGING_DEBUG
    printf("%.*s", (int)len, text);
#else
    (void)text;
    (void)len;
#endif
}

void floss_hal_debug_pin(uint8_t pin, bool level)
{
    (void)pin;
    (void)level;
}

// Pairing storage: FLOSS's small key/value store, one image in the first sector of the Bluetooth
// flash bank, mirrored in RAM. Only written when a pairing completes or a key changes.
#define BT_STORE_MAGIC     0x53534C46u // "FLSS"
#define BT_STORE_VERSION   1u
#define BT_STORE_ENTRIES   16u
#define BT_STORE_VALUE_MAX 64u
#define BT_STORE_OFFSET    BT_FLASH_BANK_OFFSET

typedef struct
{
    uint16_t key;
    uint8_t used;
    uint8_t len;
    uint8_t value[BT_STORE_VALUE_MAX];
} bt_store_entry_s;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t entry_count;
    uint32_t checksum; // FNV-1a over entries
    uint32_t reserved;
    bt_store_entry_s entries[BT_STORE_ENTRIES];
} bt_store_image_s;

#define BT_STORE_IMAGE_BYTES (((sizeof(bt_store_image_s) + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE)

static union
{
    bt_store_image_s img;
    uint8_t raw[BT_STORE_IMAGE_BYTES];
} _bt_store __attribute__((aligned(4)));

static bool _bt_store_loaded = false;

static uint32_t _bt_store_checksum(const bt_store_image_s *img)
{
    const uint8_t *p = (const uint8_t *)img->entries;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(img->entries); i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static void _bt_store_load(void)
{
    if (_bt_store_loaded)
        return;
    _bt_store_loaded = true;

    const bt_store_image_s *flash = (const bt_store_image_s *)(XIP_BASE + BT_STORE_OFFSET);
    if (flash->magic == BT_STORE_MAGIC && flash->version == BT_STORE_VERSION &&
        flash->entry_count == BT_STORE_ENTRIES && flash->checksum == _bt_store_checksum(flash))
        memcpy(&_bt_store.img, flash, sizeof(bt_store_image_s));
    else
        memset(&_bt_store, 0, sizeof(_bt_store)); // Blank, or BTstack's data from an older build
}

// Interrupts off and core 0 parked (flash_safe_execute)
static void _bt_store_flash_op(void *param)
{
    (void)param;
    flash_range_erase(BT_STORE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(BT_STORE_OFFSET, _bt_store.raw, sizeof(_bt_store.raw));
}

static floss_err_t _bt_store_commit(void)
{
    _bt_store.img.magic = BT_STORE_MAGIC;
    _bt_store.img.version = BT_STORE_VERSION;
    _bt_store.img.entry_count = BT_STORE_ENTRIES;
    _bt_store.img.checksum = _bt_store_checksum(&_bt_store.img);
    return (flash_safe_execute(_bt_store_flash_op, NULL, 100) == PICO_OK) ? FLOSS_OK : FLOSS_ERR_IO;
}

static bt_store_entry_s *_bt_store_find(uint16_t key)
{
    for (unsigned i = 0; i < BT_STORE_ENTRIES; i++)
    {
        bt_store_entry_s *e = &_bt_store.img.entries[i];
        if (e->used && e->key == key)
            return e;
    }
    return NULL;
}

int floss_hal_store_read(uint16_t key, void *buf, uint16_t len)
{
    _bt_store_load();
    const bt_store_entry_s *e = _bt_store_find(key);
    if (!e)
        return FLOSS_ERR_NOT_FOUND;
    uint16_t n = len < e->len ? len : e->len;
    memcpy(buf, e->value, n);
    return n;
}

floss_err_t floss_hal_store_write(uint16_t key, const void *buf, uint16_t len)
{
    _bt_store_load();
    if (len > BT_STORE_VALUE_MAX || (len && !buf))
        return FLOSS_ERR_INVALID;

    bt_store_entry_s *e = _bt_store_find(key);
    if (e && e->len == len && !memcmp(e->value, buf, len))
        return FLOSS_OK; // Unchanged, spare the flash

    for (unsigned i = 0; !e && i < BT_STORE_ENTRIES; i++)
    {
        if (!_bt_store.img.entries[i].used)
            e = &_bt_store.img.entries[i];
    }
    if (!e)
        return FLOSS_ERR_NOMEM;

    memset(e, 0, sizeof(*e));
    e->key = key;
    e->used = 1;
    e->len = (uint8_t)len;
    if (len)
        memcpy(e->value, buf, len);
    return _bt_store_commit();
}

floss_err_t floss_hal_store_delete(uint16_t key)
{
    _bt_store_load();
    bt_store_entry_s *e = _bt_store_find(key);
    if (!e)
        return FLOSS_OK;
    memset(e, 0, sizeof(*e));
    return _bt_store_commit();
}

// cyw43_arch sets up BTstack's run loop when the radio firmware is enabled. FLOSS runs from the
// transport task instead, so there is nothing to set up.
bool btstack_cyw43_init(async_context_t *context)
{
    (void)context;
    return true;
}

void btstack_cyw43_deinit(async_context_t *context)
{
    (void)context;
}

/***********************************************/
/********* Timing and limits *******************/

#define BT_HAL_TARGET_POLLING_RATE_MS 8
// A real Wii Remote reports every 10 ms
#define BT_HAL_WII_POLLING_RATE_MS 10
#define BT_HAL_INBOUND_FIFO_LEN 32

// A paired Wii Remote connects to the console, never the reverse, so keep paging the saved
// Wii until it answers (it may still be booting).
#define BT_HAL_WII_RECONNECT_MS 1000

// Page timeout while reconnecting to a Wii (x 0.625 ms, ~5 s) so retries during a reload stay short
#define BT_HAL_WII_PAGE_TIMEOUT 0x2000

// When a title starts the Wii reloads its system software and every link drops. Real remotes
// reconnect on their own, so after a lost link keep paging for this long before powering off.
#define BT_HAL_WII_LINK_LOST_WINDOW_MS 60000

// A Wii going to standby takes the page at the radio (it switches roles) and then never completes
// the connection. While the Wii reloads for a title, a page has completed within ~3.5 s of the
// role switch; leaving the Homebrew Channel can take longer, so allow 10 s.
#define BT_HAL_WII_PAGE_STALL_MS 10000

// After the Wii closes the HID channels, give a normal disconnect this long before cycling
#define BT_HAL_WII_TEARDOWN_MS 1500

// Quitting or launching a title can also leave the Wii holding the link without taking our
// reports. If no report has gone out for this long, cycle the radio. With several remotes
// connected the Wii can take a second or two to service a link, so this stays well above that.
#define BT_HAL_WII_STALL_MS       6000
#define BT_HAL_WII_STALL_CHECK_MS 500

// The host may be busy setting up another controller and miss our call, and neither the Switch
// nor Windows calls a controller back. Try again a few times, at random gaps so controllers
// turned on together stop colliding.
#define BT_HAL_RECONNECT_TRIES     5
#define BT_HAL_RECONNECT_MIN_MS    1000
#define BT_HAL_RECONNECT_SPREAD_MS 2000

// Auto mode: hosts listen for calls every 1.28 s, so each gets two of those windows
#define BT_HAL_PROBE_PAGE_TIMEOUT   0x1000 // x 0.625 ms, 2.56 s per host
#define BT_HAL_PROBE_ANSWER_MS      1500
#define BT_HAL_DEFAULT_PAGE_TIMEOUT 0x6000 // As the BTstack backend

#define BT_HAL_GAP_IAC_GENERAL 0x9E8B33u
#define BT_HAL_NO_HANDLE 0xFFFFu

/***********************************************/
/********* State *******************************/

static core_params_s *_bt_hal_params = NULL;
static const core_hid_device_t *_bt_hal_hid = NULL;

static floss_hid_config_t _hid_cfg;
static uint8_t _hid_record[700];

static bool _bt_stack_up = false;   // The radio comes up once per boot
static bool _bt_mode_ready = false; // A mode is set up (Auto mode may probe hosts first)
static bool _mode_wii = false;
static bool _mode_sinput = false;
static uint32_t _report_interval_us = BT_HAL_TARGET_POLLING_RATE_MS * 1000;

static volatile bool _connected = false; // HID session up
static bool _pairing_mode = false;
static bool _discoverable = false;

// The address the radio runs with, and a restart (power cycle) waiting to run from the task
static uint8_t _radio_mac[6];
static uint8_t _restart_mac[6];
static bool _restart_pending = false;

// Called from Bluetooth callbacks: the shutdown itself runs from transport_bt_task
static volatile bool _shutdown_pending = false;

typedef struct
{
    uint8_t len;
    uint8_t data[64];
} bt_hal_inbound_report_s;

HOJA_CROSSCORE_FIFO_TYPE(bt_inbound, bt_hal_inbound_report_s, BT_HAL_INBOUND_FIFO_LEN);
static hoja_fifo_bt_inbound_t _bt_inbound_fifo;

// Reports: one per interval, built only when it can leave
static floss_timer_t _report_timer;
static uint32_t _report_deadline_us = 0;
static bool _send_pending = false;

// Switch and SInput reconnect retries
static floss_timer_t _reconnect_timer;
static uint8_t _reconnect_tries = 0;

// Wii
static floss_timer_t _wii_reconnect_timer;
static floss_timer_t _wii_page_stall_timer;
static floss_timer_t _wii_teardown_timer;
static floss_timer_t _wii_stall_timer;
static uint32_t _wii_link_lost_deadline_ms = 0; // 0 while connected or before the first link
static uint16_t _wii_acl_handle = BT_HAL_NO_HANDLE;
static uint16_t _wii_teardown_handle = BT_HAL_NO_HANDLE;
static uint32_t _wii_last_report_ms = 0;
static bool _wii_radio_cycling = false;
static volatile bool _wii_stale_link = false;

// Powering off, the Wii only closes the HID channels (as when quitting a title), so reconnecting
// starts as usual. A Wii in standby still accepts a new link but never answers the HID channel
// request, while a reloading Wii refuses it outright until it is ready. That tells them apart: on
// the standby answer, power off like a real remote instead of paging again.
static bool _wii_fresh_acl = false;  // This reconnect attempt brought up a new link
static bool _wii_page_pending = false;
static bool _wii_page_answered = false; // ...and the Wii's radio has taken it

// Auto mode probe
typedef struct
{
    core_reportformat_t format;
    const uint8_t *addr;
    uint8_t local_mac[6]; // Our address in that mode: hosts only answer the remote they paired
} bt_hal_probe_host_s;

static bt_hal_probe_host_s _probe_hosts[3];
static uint8_t _probe_count = 0;
static uint8_t _probe_index = 0;
static bool _probe_running = false;
static floss_l2cap_chan_t *_probe_chan = NULL;
static uint16_t _probe_handle = BT_HAL_NO_HANDLE;
static floss_timer_t _probe_timer;

static void _bt_hal_mode_setup(core_params_s *params);
static void _bt_hal_restart(void);
static void _bt_hal_restart_as(const uint8_t *mac);
static void _bt_hal_probe_next(void);

/***********************************************/
/********* Helpers *****************************/

static uint32_t _bt_hal_now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

// HOJA keeps addresses most significant byte first, FLOSS as on the air (least significant first)
static void _bt_hal_addr_from_hoja(const uint8_t *mac, floss_addr_t *out)
{
    for (int i = 0; i < 6; i++)
        out->b[i] = mac[5 - i];
}

static void _bt_hal_addr_to_hoja(const floss_addr_t *addr, uint8_t *mac)
{
    for (int i = 0; i < 6; i++)
        mac[i] = addr->b[5 - i];
}

static inline void _bt_hal_reverse_bytes(const uint8_t *in, uint8_t *out, size_t len)
{
    for (size_t i = 0; i < len; i++)
        out[i] = in[len - 1 - i];
}

/** True when persisted pairing bytes are not blank (0x0000) or erased (0xFFFF…) sentinel. */
static bool _bt_hal_is_stored_identity_valid(const uint8_t *bytes)
{
    if (bytes[0] == 0xFF && bytes[1] == 0xFF)
        return false;
    if (!bytes[0] && !bytes[1])
        return false;
    return true;
}

static bool _bt_hal_is_wii(void)
{
    return core_current_reportformat() == CORE_REPORTFORMAT_WII;
}

// Connectable whenever no host is connected; discoverable when asked. A connected controller
// stops answering searches and pages, like a real one (on a Wii, a connected remote that stayed
// findable stalled pairing another remote, or locked it up).
static void _bt_hal_apply_scan(void)
{
    if (_connected)
        floss_gap_set_scan(false, false);
    else
        floss_gap_set_scan(_discoverable, true);
}

static void _bt_hal_set_discoverable(bool discoverable)
{
    _discoverable = discoverable;
    _bt_hal_apply_scan();
}

static void _bt_hal_write_page_timeout(uint16_t slots)
{
    uint8_t p[2];
    floss_put_le16(p, slots);
    floss_hci_cmd(FLOSS_HCI_WRITE_PAGE_TIMEOUT, p, sizeof(p), NULL, NULL);
}

// The Wii's SYNC search only finds devices listening on the Limited Inquiry Access Code, so in
// Wii mode answer both it and the general code
static void _bt_hal_wii_write_iac(void)
{
    uint8_t p[7];
    p[0] = 2;
    floss_put_le24(&p[1], NWII_HID_INQUIRY_ACCESS_CODE);
    floss_put_le24(&p[4], BT_HAL_GAP_IAC_GENERAL);
    floss_hci_cmd(FLOSS_HCI_WRITE_CURRENT_IAC_LAP, p, sizeof(p), NULL, NULL);
}

static void _bt_hal_shutdown(void)
{
    _shutdown_pending = true;
}

/***********************************************/
/********* Reports *****************************/

static void _bt_hal_queue_inbound_report(const uint8_t *report, uint16_t len)
{
    if (!report || len == 0)
        return;

    bt_hal_inbound_report_s inbound = {0};
    if (len > sizeof(inbound.data))
        len = sizeof(inbound.data);

    memcpy(inbound.data, report, len);
    inbound.len = (uint8_t)len;
    (void)hoja_fifo_bt_inbound_push(&_bt_inbound_fifo, &inbound);
}

// Output reports, from the interrupt channel or SET_REPORT on control: [report id, data...]
static void _bt_hal_on_output_report(uint8_t report_id, const uint8_t *data, uint16_t len)
{
    uint8_t report[64];
    if (len > sizeof(report) - 1)
        len = sizeof(report) - 1;
    report[0] = report_id;
    memcpy(&report[1], data, len);
    _bt_hal_queue_inbound_report(report, (uint16_t)(len + 1));
}

// The core builds the report at the moment it can leave, as with BTstack's can-send-now
#if HOJA_BT_FLOSS_DIAG
// Timing diagnostics for test builds: SInput input reports carry the send time and link state in
// their reserved bytes, and the report interval steps through _diag_intervals_us.
#define BT_DIAG_OFFSET   47 // reserved_bulk, after the report ID
#define BT_DIAG_STEP_US  15000000u
static const uint32_t _diag_intervals_us[] = {8000, 4000, 2000, 1000};
static uint8_t _diag_step = 0;
static uint32_t _diag_step_at_us = 0;
static uint32_t _diag_tick_due_us = 0;
static uint16_t _diag_deferred = 0;
static bool _diag_defer_counted = false;
static uint32_t _diag_task_last_us = 0;
static uint32_t _diag_task_gap_max_us = 0;
static uint32_t _diag_gen_us = 0;      // Building this report
static uint32_t _diag_send_max_us = 0; // Handing reports to the stack, longest since the last report
static uint32_t _diag_poll_max_us = 0; // floss_poll without the report work inside it
static uint32_t _diag_nested_us = 0;   // Report work done inside the current floss_poll

static void _bt_hal_diag_put_us(uint8_t *out, uint32_t us)
{
    floss_put_le16(out, us > 0xFFFFu ? 0xFFFFu : (uint16_t)us);
}
static uint8_t _diag_max_slots = 0;

// Max Slots Change: handle(2) max slots(1)
static void _bt_hal_diag_snoop(uint8_t type, bool is_rx, const uint8_t *pkt, uint16_t len, uint32_t t_us)
{
    (void)t_us;
    if (type == FLOSS_PKT_EVT && is_rx && len >= 5 && pkt[0] == FLOSS_EVT_MAX_SLOTS_CHANGE)
        _diag_max_slots = pkt[4];
}

static void _bt_hal_diag_task_name(uint8_t *out, const char *name)
{
    out[0] = (name && name[0]) ? (uint8_t)name[0] : '-';
    out[1] = (name && name[0] && name[1]) ? (uint8_t)name[1] : ' ';
}

static void _bt_hal_diag_fill(uint8_t *data, uint32_t now)
{
    const floss_conn_t *c = floss_hci_conn_by_handle(floss_hid_handle());
    uint32_t late = now - _diag_tick_due_us;
    uint8_t *d = &data[BT_DIAG_OFFSET];
    d[0] = 0xD1;
    d[1] = (uint8_t)(_report_interval_us / 100u);
    d[2] = c ? c->mode : 0xFF;
    floss_put_le16(&d[3], c ? c->sniff_interval : 0);
    d[5] = c ? (uint8_t)c->inflight : 0xFF;
    d[6] = (uint8_t)floss_hci_acl_credits();
    floss_put_le16(&d[7], late > 0xFFFFu ? 0xFFFFu : (uint16_t)late);
    floss_put_le16(&d[9], _diag_deferred);
    floss_put_le16(&d[11], _diag_task_gap_max_us > 0xFFFFu ? 0xFFFFu : (uint16_t)_diag_task_gap_max_us);
    floss_put_le32(&d[13], now);
    _diag_task_gap_max_us = 0;

    // Touchpad bytes (unused on this board): the two longest tasks, and the link's max slots
    const char *n1, *n2;
    uint32_t us1, us2;
    tasks_diag_take_worst(&n1, &us1, &n2, &us2);
    uint8_t *t = &data[35];
    _bt_hal_diag_task_name(&t[0], n1);
    floss_put_le16(&t[2], us1 > 0xFFFFu ? 0xFFFFu : (uint16_t)us1);
    (void)n2;
    (void)us2;
    _bt_hal_diag_put_us(&t[4], _diag_gen_us);
    _bt_hal_diag_put_us(&t[6], _diag_send_max_us);
    t[8] = _diag_max_slots;
    _bt_hal_diag_put_us(&t[9], _diag_poll_max_us);
    _diag_send_max_us = 0;
    _diag_poll_max_us = 0;
}

// Next interval every BT_DIAG_STEP_US while connected
static void _bt_hal_diag_step(uint32_t now)
{
    if (!_connected)
    {
        _diag_step_at_us = 0;
        return;
    }
    if (!_diag_step_at_us)
    {
        _diag_step = 0;
        _diag_step_at_us = now + BT_DIAG_STEP_US;
        _report_interval_us = _diag_intervals_us[0];
        return;
    }
    if (floss_time_diff(now, _diag_step_at_us) >= 0)
    {
        _diag_step = (uint8_t)((_diag_step + 1) % (sizeof(_diag_intervals_us) / sizeof(_diag_intervals_us[0])));
        _report_interval_us = _diag_intervals_us[_diag_step];
        _diag_step_at_us = now + BT_DIAG_STEP_US;
    }
}
#endif

static void _bt_hal_try_send(void)
{
    if (!_send_pending || !_connected)
        return;

    if (!floss_hid_can_send_now())
    {
#if HOJA_BT_FLOSS_DIAG
        if (!_diag_defer_counted)
        {
            _diag_defer_counted = true;
            _diag_deferred++;
        }
#endif
        return;
    }

    _send_pending = false;
    _wii_last_report_ms = _bt_hal_now_ms();

    core_report_s report = {0};
#if HOJA_BT_FLOSS_DIAG
    uint32_t gen_start = time_us_32();
#endif
    if (core_get_generated_report(&report) && report.size >= 1)
    {
#if HOJA_BT_FLOSS_DIAG
        _diag_gen_us = time_us_32() - gen_start;
        _diag_nested_us += _diag_gen_us;
#endif
#if HOJA_BT_FLOSS_DIAG
        _diag_defer_counted = false;
        if (_mode_sinput && report.data[0] == 1 && report.size >= BT_DIAG_OFFSET + 17)
            _bt_hal_diag_fill(report.data, floss_hal_time_us());
#endif
        // Byte 0 is the report ID
#if HOJA_BT_FLOSS_DIAG
        uint32_t send_start = time_us_32();
#endif
        floss_hid_send_input_now(report.data[0], &report.data[1], (uint16_t)(report.size - 1));
#if HOJA_BT_FLOSS_DIAG
        uint32_t send_us = time_us_32() - send_start;
        _diag_nested_us += send_us;
        if (send_us > _diag_send_max_us)
            _diag_send_max_us = send_us;
#endif
        tasks_mark_sent_isr();
    }
}

// Fixed deadlines so tick latency doesn't stretch the interval
static void _bt_hal_report_tick(floss_timer_t *t, void *arg)
{
    (void)arg;
    if (!_connected)
        return;

    uint32_t now = floss_hal_time_us();
#if HOJA_BT_FLOSS_DIAG
    _diag_tick_due_us = _report_deadline_us;
    _bt_hal_diag_step(now);
#endif
    _report_deadline_us += _report_interval_us;
    if (floss_time_diff(now, _report_deadline_us) >= 0)
    {
        // A whole interval behind, restart from now instead of bursting
        _report_deadline_us = now + _report_interval_us;
    }
    floss_timer_start_at(t, _report_deadline_us);

    _send_pending = true;
    _bt_hal_try_send();
}

static void _bt_hal_report_timer_start(void)
{
    _report_deadline_us = floss_hal_time_us() + _report_interval_us;
    _send_pending = true;
    floss_timer_start_at(&_report_timer, _report_deadline_us);
}

/***********************************************/
/********* Wii link handling *******************/

// A reloading Wii keeps transmitting but stops acknowledging us, so a normal disconnect waits out
// the 30 s LMP response timeout. Restarting our radio drops the link locally at once. The Wii is
// the master, so its own supervision timeout decides when it lets go of the old link; the restart
// keeps paging the Wii until it does.
static void _bt_hal_wii_radio_cycle(void)
{
    if (_wii_radio_cycling)
        return;

    _wii_radio_cycling = true;
    _wii_link_lost_deadline_ms = _bt_hal_now_ms() + BT_HAL_WII_LINK_LOST_WINDOW_MS;
    _bt_hal_restart();
}

static void _bt_hal_wii_page_stall_handler(floss_timer_t *t, void *arg)
{
    (void)t;
    (void)arg;
    if (_wii_page_pending && _wii_page_answered && _wii_link_lost_deadline_ms)
    {
        // Wii is going to standby
        _wii_link_lost_deadline_ms = 0;
        floss_timer_stop(&_wii_reconnect_timer);
        _bt_hal_shutdown();
    }
    _wii_page_pending = false;
}

static void _bt_hal_wii_reconnect_timer_start(void)
{
    floss_timer_start(&_wii_reconnect_timer, BT_HAL_WII_RECONNECT_MS * 1000u);
}

static void _bt_hal_wii_reconnect_handler(floss_timer_t *t, void *arg)
{
    (void)t;
    (void)arg;
    if (_connected)
        return;

    if (_wii_link_lost_deadline_ms && (int32_t)(_bt_hal_now_ms() - _wii_link_lost_deadline_ms) >= 0)
    {
        // The Wii never came back
        _wii_link_lost_deadline_ms = 0;
        _bt_hal_shutdown();
        return;
    }

    // The last session may still be closing
    if (floss_hid_state() != FLOSS_HID_STATE_IDLE)
    {
        _bt_hal_wii_reconnect_timer_start();
        return;
    }

    _wii_fresh_acl = false;
    if (_wii_link_lost_deadline_ms)
    {
        _wii_page_pending = true;
        _wii_page_answered = false;
        floss_timer_start(&_wii_page_stall_timer, BT_HAL_WII_PAGE_STALL_MS * 1000u);
    }

    floss_addr_t addr;
    _bt_hal_addr_from_hoja(gamepad_config->host_mac_wii, &addr);
    if (floss_hid_connect(&addr) != FLOSS_OK)
        _bt_hal_wii_reconnect_timer_start();
}

static void _bt_hal_wii_teardown_handler(floss_timer_t *t, void *arg)
{
    (void)t;
    (void)arg;
    // Only if that same link is still up; a fresh reconnect may already have replaced it
    if (_wii_teardown_handle != BT_HAL_NO_HANDLE && _wii_teardown_handle == _wii_acl_handle)
        _bt_hal_wii_radio_cycle();
    _wii_teardown_handle = BT_HAL_NO_HANDLE;
}

static void _bt_hal_wii_stall_handler(floss_timer_t *t, void *arg)
{
    (void)arg;
    if (_connected && (_bt_hal_now_ms() - _wii_last_report_ms) > BT_HAL_WII_STALL_MS)
        _bt_hal_wii_radio_cycle();

    floss_timer_start(t, BT_HAL_WII_STALL_CHECK_MS * 1000u);
}

// When a title starts, the Wii reloads its system software but keeps the link, and its new stack
// addresses L2CAP channels that no longer exist. Drop the stale link and reconnect straight away,
// as a real remote does. The restart runs from the task, outside the stack.
static void _bt_hal_wii_unknown_cid(uint16_t handle, uint16_t cid)
{
    (void)handle;
    (void)cid;
    _wii_stale_link = true;
}

/***********************************************/
/********* Reconnecting ************************/

// Reconnect to the host saved for this mode
static void _bt_hal_reconnect_saved_host(void)
{
    floss_addr_t addr;

    switch (core_current_reportformat())
    {
    default:
    case CORE_REPORTFORMAT_SWPRO:
    {
        if (!_bt_hal_is_stored_identity_valid(switchpair_config->link_key))
        {
            _bt_hal_set_discoverable(true);
            return;
        }

        // The Switch pairs over USB: hand its key to the stack (big-endian in settings, HCI order
        // in the stack)
        _bt_hal_addr_from_hoja(gamepad_config->host_mac_switch, &addr);
        floss_link_key_t key;
        _bt_hal_reverse_bytes(switchpair_config->link_key, key.key, 16);
        key.type = 0x04; // Unauthenticated combination key from P-192

        const floss_bond_t *bond = floss_gap_bond_find(&addr);
        if (!bond || memcmp(bond->key.key, key.key, 16))
            floss_gap_bond_add(&addr, &key);

        floss_hid_connect(&addr);
    }
    break;

    case CORE_REPORTFORMAT_SINPUT:
        if (_bt_hal_is_stored_identity_valid(gamepad_config->host_mac_sinput))
        {
            _bt_hal_addr_from_hoja(gamepad_config->host_mac_sinput, &addr);
            floss_hid_connect(&addr);
        }
        break;

    case CORE_REPORTFORMAT_WII:
        // Stay discoverable either way: the Wii can still SYNC or temporarily connect to us while
        // we page the saved console
        _bt_hal_set_discoverable(true);
        if (_bt_hal_is_stored_identity_valid(gamepad_config->host_mac_wii))
        {
            _bt_hal_addr_from_hoja(gamepad_config->host_mac_wii, &addr);
            floss_hid_connect(&addr);
        }
        break;
    }
}

static void _bt_hal_reconnect_handler(floss_timer_t *t, void *arg)
{
    (void)t;
    (void)arg;
    if (_connected || _pairing_mode || floss_hid_state() != FLOSS_HID_STATE_IDLE)
        return;
    _bt_hal_reconnect_saved_host();
}

// After a failed reconnect in Switch or SInput mode (Wii mode has its own)
static void _bt_hal_reconnect_retry(void)
{
    if (_pairing_mode || _reconnect_tries >= BT_HAL_RECONNECT_TRIES)
        return;
    _reconnect_tries++;
    floss_timer_start(&_reconnect_timer,
                      (BT_HAL_RECONNECT_MIN_MS + get_rand_32() % BT_HAL_RECONNECT_SPREAD_MS) * 1000u);
}

/***********************************************/
/********* Auto mode host probe ****************/

// Before reconnecting, call each saved host and open a service discovery channel (no pairing, no
// HID). An awake host answers, even if only to refuse. A Wii in standby takes the call but never
// answers. The first awake console picks the mode, Switch, then Wii. The PC isn't called: with
// neither console awake, SInput reconnects to it directly, which saves a whole call.

static void _bt_hal_probe_add(core_reportformat_t format, const uint8_t *addr)
{
    if (!_bt_hal_is_stored_identity_valid(addr))
        return;

    // A host saved for two modes answers for both: the first mode keeps it
    for (uint8_t i = 0; i < _probe_count; i++)
    {
        if (!memcmp(addr, _probe_hosts[i].addr, 6))
            return;
    }

    _probe_hosts[_probe_count].format = format;
    _probe_hosts[_probe_count].addr = addr;
    transport_mode_mac(_probe_hosts[_probe_count].local_mac, format);
    _probe_count++;
}

static void _bt_hal_probe_finish(core_reportformat_t found)
{
    _probe_running = false;
    _bt_hal_write_page_timeout(BT_HAL_DEFAULT_PAGE_TIMEOUT);

    // No console answered: a saved PC gets SInput, otherwise Switch mode waits for the Switch
    if (found == CORE_REPORTFORMAT_UNDEFINED && _bt_hal_is_stored_identity_valid(gamepad_config->host_mac_sinput))
        found = CORE_REPORTFORMAT_SINPUT;

    // Switch mode carries on: set it up now and bring the radio back with it. Otherwise core 1
    // brings up the mode that answered.
    if (autodetect_bt_probe_result(found))
    {
        _bt_hal_mode_setup(core_current_params());
        _bt_hal_restart();
    }
}

static void _bt_hal_probe_host_done(bool awake)
{
    floss_timer_stop(&_probe_timer);
    if (_probe_chan)
    {
        floss_l2cap_chan_t *chan = _probe_chan;
        _probe_chan = NULL;
        floss_l2cap_disconnect(chan);
    }

    if (awake)
    {
        _bt_hal_probe_finish(_probe_hosts[_probe_index].format);
        return;
    }

    if (_probe_handle != BT_HAL_NO_HANDLE)
        floss_gap_disconnect(_probe_handle, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
    _probe_handle = BT_HAL_NO_HANDLE;

    _probe_index++;
    _bt_hal_probe_next();
}

// Connected but no answer: asleep
static void _bt_hal_probe_timeout(floss_timer_t *t, void *arg)
{
    (void)t;
    (void)arg;
    if (_probe_running)
        _bt_hal_probe_host_done(false);
}

static void _bt_hal_probe_on_open(floss_l2cap_chan_t *chan, uint16_t status)
{
    if (!_probe_running || chan != _probe_chan)
        return;
    if (status != 0)
        _probe_chan = NULL;

    // Opened, or refused by the host itself: awake. A failed call or no answer: not
    const bool answered = (status == 0) ||
                          (status >= FLOSS_L2CAP_CONN_PSM_UNSUPP && status <= FLOSS_L2CAP_CONN_NO_RESOURCES);
    _bt_hal_probe_host_done(answered);
}

static void _bt_hal_probe_on_close(floss_l2cap_chan_t *chan)
{
    if (chan == _probe_chan)
        _probe_chan = NULL;
}

static void _bt_hal_probe_on_data(floss_l2cap_chan_t *chan, const uint8_t *data, uint16_t len)
{
    (void)chan;
    (void)data;
    (void)len;
}

static const floss_l2cap_ops_t _probe_ops = {
    .on_open = _bt_hal_probe_on_open,
    .on_close = _bt_hal_probe_on_close,
    .on_data = _bt_hal_probe_on_data,
};

// The call went through: open the channel and wait for the answer
static void _bt_hal_probe_on_connected(const floss_gap_evt_t *evt)
{
    if (!_probe_running || _probe_index >= _probe_count)
        return;

    floss_addr_t addr;
    _bt_hal_addr_from_hoja(_probe_hosts[_probe_index].addr, &addr);
    if (!floss_addr_eq(&evt->addr, &addr))
        return;

    _probe_handle = evt->handle;
    floss_timer_start(&_probe_timer, BT_HAL_PROBE_ANSWER_MS * 1000u);
    if (floss_l2cap_connect(evt->handle, FLOSS_PSM_SDP, &_probe_ops, NULL, FLOSS_SEC_NONE, &_probe_chan) != FLOSS_OK)
    {
        _probe_chan = NULL;
        _bt_hal_probe_host_done(false);
    }
}

static void _bt_hal_probe_on_connect_failed(const floss_gap_evt_t *evt)
{
    if (!_probe_running || _probe_index >= _probe_count || _probe_handle != BT_HAL_NO_HANDLE)
        return;

    floss_addr_t addr;
    _bt_hal_addr_from_hoja(_probe_hosts[_probe_index].addr, &addr);
    if (floss_addr_eq(&evt->addr, &addr))
        _bt_hal_probe_host_done(false);
}

static void _bt_hal_probe_next(void)
{
    if (_probe_index >= _probe_count)
    {
        _bt_hal_probe_finish(CORE_REPORTFORMAT_UNDEFINED);
        return;
    }

    // Call each host from the address it paired with. The probe carries on once the radio is back.
    if (memcmp(_radio_mac, _probe_hosts[_probe_index].local_mac, 6))
    {
        _bt_hal_restart_as(_probe_hosts[_probe_index].local_mac);
        return;
    }

    _bt_hal_write_page_timeout(BT_HAL_PROBE_PAGE_TIMEOUT);

    floss_addr_t addr;
    _bt_hal_addr_from_hoja(_probe_hosts[_probe_index].addr, &addr);
    _probe_handle = BT_HAL_NO_HANDLE;
    if (floss_gap_connect(&addr) != FLOSS_OK)
        _bt_hal_probe_host_done(false);
}

// True while the probe runs, the reconnect follows from its result
static bool _bt_hal_probe_start(void)
{
    // Back from changing address for the next host
    if (_probe_running)
    {
        _bt_hal_probe_next();
        return true;
    }
    if (!autodetect_bt_probe_pending())
        return false;

    _probe_count = 0;
    _probe_index = 0;
    _bt_hal_probe_add(CORE_REPORTFORMAT_SWPRO, gamepad_config->host_mac_switch);
    _bt_hal_probe_add(CORE_REPORTFORMAT_WII, gamepad_config->host_mac_wii);

    if (!_probe_count)
    {
        _bt_hal_probe_finish(CORE_REPORTFORMAT_UNDEFINED);
        return true;
    }

    _probe_running = true;
    _bt_hal_probe_next();
    return true;
}

/***********************************************/
/********* HID callbacks ***********************/

static void _bt_hal_on_connected(const floss_addr_t *host)
{
    floss_timer_stop(&_wii_reconnect_timer);

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

    uint8_t mac[6];
    _bt_hal_addr_to_hoja(host, mac);
    if (memcmp(mac, addr_location, 6))
    {
        // New address, save
        memcpy(addr_location, mac, 6);
        settings_commit_blocks();
    }

    // Paired or reconnected: from now on a dropped link pages this host again
    if (_bt_hal_is_wii())
        _pairing_mode = false;

    _wii_link_lost_deadline_ms = 0;
    _wii_last_report_ms = _bt_hal_now_ms();
    if (core_current_params()->core_connected)
        core_current_params()->core_connected();

    _connected = true;
    _discoverable = false;
    _bt_hal_apply_scan();
    _bt_hal_report_timer_start();
}

// The HID channels closed. Called once per session: when FLOSS leaves the connected state (the
// host closed the channels, the link may still be up) or when the link drops.
static void _bt_hal_session_closed(void)
{
    if (!_connected)
        return;

    _connected = false;
    _send_pending = false;
    floss_timer_stop(&_report_timer);

    // Torn down by our own restart: the stack comes back and reconnects
    if (_restart_pending)
        return;

    if (_bt_hal_is_wii() && _bt_hal_is_stored_identity_valid(gamepad_config->host_mac_wii))
    {
        // Likely a title launch; reconnect unless the Wii says it is powering off (handled on
        // the disconnect). When the Wii only closed the HID channels, its restarted stack will
        // not answer on the old link, so drop it and page fresh; if the Wii does not acknowledge
        // the disconnect, restart the radio instead of waiting.
        if (_wii_acl_handle != BT_HAL_NO_HANDLE)
        {
            floss_gap_disconnect(_wii_acl_handle, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
            _wii_teardown_handle = _wii_acl_handle;
            floss_timer_start(&_wii_teardown_timer, BT_HAL_WII_TEARDOWN_MS * 1000u);
        }
        _wii_link_lost_deadline_ms = _bt_hal_now_ms() + BT_HAL_WII_LINK_LOST_WINDOW_MS;
        // Findable again while reconnecting, so the Wii can SYNC or connect to us
        _bt_hal_set_discoverable(true);
        _bt_hal_wii_reconnect_timer_start();
        return;
    }

    _bt_hal_apply_scan();
    _bt_hal_shutdown();
}

static void _bt_hal_on_disconnected(uint8_t reason)
{
    (void)reason;
    _bt_hal_session_closed();
}

static void _bt_hal_on_connect_failed(uint8_t reason, uint16_t channel_status)
{
    (void)reason;
    _connected = false;
    _bt_hal_set_discoverable(true);

    // Reconnecting after a lost link, the Wii took a new link but never answered: it has gone to
    // standby
    if (_bt_hal_is_wii() && _wii_link_lost_deadline_ms && _wii_fresh_acl &&
        channel_status == FLOSS_L2CAP_STATUS_TIMEOUT)
    {
        _wii_link_lost_deadline_ms = 0;
        floss_timer_stop(&_wii_reconnect_timer);
        _bt_hal_shutdown();
        return;
    }

    if (_bt_hal_is_wii() && !_pairing_mode && _bt_hal_is_stored_identity_valid(gamepad_config->host_mac_wii))
        _bt_hal_wii_reconnect_timer_start();
    else if (!_bt_hal_is_wii())
        _bt_hal_reconnect_retry();
}

static const floss_hid_callbacks_t _bt_hal_hid_callbacks = {
    .on_connected = _bt_hal_on_connected,
    .on_disconnected = _bt_hal_on_disconnected,
    .on_output_report = _bt_hal_on_output_report,
    .on_connect_failed = _bt_hal_on_connect_failed,
};

// Wii SYNC pairing is legacy PIN pairing; the PIN is the Wii's address reversed
static uint8_t _bt_hal_pin_handler(const floss_addr_t *peer, uint8_t pin[16])
{
    if (_bt_hal_is_wii())
    {
        uint8_t mac[6];
        _bt_hal_addr_to_hoja(peer, mac);
        nwii_hid_make_pin(mac, pin);
        return NWII_HID_PIN_LEN;
    }
    memcpy(pin, "0000", 4);
    return 4;
}

/***********************************************/
/********* GAP events **************************/

static void _bt_hal_on_ready(void)
{
    _wii_radio_cycling = false;

    if (_bt_mode_ready)
    {
        _bt_hal_write_page_timeout(_mode_wii ? BT_HAL_WII_PAGE_TIMEOUT : BT_HAL_DEFAULT_PAGE_TIMEOUT);
        if (_mode_wii)
            _bt_hal_wii_write_iac();
    }

    if (_pairing_mode)
    {
        _bt_hal_set_discoverable(true);
        return;
    }
    else if (!_bt_mode_ready && !autodetect_bt_probe_pending())
    {
        // The probe gave up before deciding: set up this mode now
        _bt_hal_mode_setup(core_current_params());
        _bt_hal_restart();
    }
    else if (!_bt_hal_probe_start())
    {
        _reconnect_tries = 0;
        _bt_hal_reconnect_saved_host();
    }
}

static void _bt_hal_gap_event(const floss_gap_evt_t *evt, void *user)
{
    (void)user;

    switch (evt->type)
    {
    case FLOSS_GAP_EVT_READY:
        _bt_hal_on_ready();
        break;

    case FLOSS_GAP_EVT_ROLE_CHANGE:
        if (_wii_page_pending && evt->status == FLOSS_HCI_SUCCESS)
            _wii_page_answered = true;
        break;

    case FLOSS_GAP_EVT_CONNECTED:
        _bt_hal_probe_on_connected(evt);
        _wii_page_pending = false;
        floss_timer_stop(&_wii_page_stall_timer);
        _wii_acl_handle = evt->handle;
        _wii_fresh_acl = true;
        if (_mode_wii)
            floss_gap_set_supervision_timeout(evt->handle, NWII_HID_LINK_SUPERVISION_TIMEOUT);
        break;

    case FLOSS_GAP_EVT_CONNECT_FAILED:
        _bt_hal_probe_on_connect_failed(evt);
        _wii_page_pending = false;
        floss_timer_stop(&_wii_page_stall_timer);
        break;

    case FLOSS_GAP_EVT_DISCONNECTED:
        if (evt->handle == _wii_acl_handle)
            _wii_acl_handle = BT_HAL_NO_HANDLE;
        if (evt->handle == _wii_teardown_handle)
        {
            floss_timer_stop(&_wii_teardown_timer);
            _wii_teardown_handle = BT_HAL_NO_HANDLE;
        }

        // A Wii that is powering off says so; follow it instead of trying to reconnect
        if (_bt_hal_is_wii() && evt->status == FLOSS_HCI_ERR_REMOTE_POWER_OFF)
        {
            floss_timer_stop(&_wii_reconnect_timer);
            _wii_link_lost_deadline_ms = 0;
            _bt_hal_shutdown();
        }
        break;

    case FLOSS_GAP_EVT_PAIRING_COMPLETE:
        // A Switch paired over Bluetooth: keep its key for USB pairing and the reconnect
        if (core_current_reportformat() == CORE_REPORTFORMAT_SWPRO)
        {
            const floss_bond_t *bond = floss_gap_bond_find(&evt->addr);
            if (bond)
            {
                ns_usbpair_s usbpair = {0};
                _bt_hal_addr_to_hoja(&evt->addr, usbpair.host_mac);
                // Big-endian, so the flash contents and debug logs stay readable
                _bt_hal_reverse_bytes(bond->key.key, usbpair.link_key, 16);
                ns_api_hook_set_usbpair(usbpair);
            }
        }
        break;

    default:
        break;
    }
}

/***********************************************/
/********* Stack lifecycle *********************/

// Everything that belongs to one mode; applied each time the stack starts
static void _bt_hal_mode_setup(core_params_s *params)
{
    _mode_wii = (params->core_report_format == CORE_REPORTFORMAT_WII);
    _mode_sinput = (params->core_report_format == CORE_REPORTFORMAT_SINPUT);
    _report_interval_us = (_mode_wii ? BT_HAL_WII_POLLING_RATE_MS : BT_HAL_TARGET_POLLING_RATE_MS) * 1000u;
    _pairing_mode = (params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0;
    _bt_mode_ready = true;
}

static bool _bt_hal_mode_apply(void)
{
    floss_hid_config_defaults(&_hid_cfg);
    _hid_cfg.name = _bt_hal_hid->name;
    _hid_cfg.report_descriptor = _bt_hal_hid->hid_report_descriptor;
    _hid_cfg.report_descriptor_len = _bt_hal_hid->hid_report_descriptor_len;
    _hid_cfg.subclass = FLOSS_HID_SUBCLASS_GAMEPAD;
    _hid_cfg.class_of_device = _mode_wii ? NWII_HID_CLASS_OF_DEVICE : 0x002508;
    _hid_cfg.vendor_id_source = 2; // USB
    _hid_cfg.vendor_id = _bt_hal_hid->vid;
    _hid_cfg.product_id = _bt_hal_hid->pid;
    _hid_cfg.version = 0x0100;
    _hid_cfg.auto_discoverable = false;
    _hid_cfg.auto_reconnect = false;
    _hid_cfg.app_controls_scan = true;
    // The Wii only does legacy PIN pairing, and authenticates on its own terms; asking for
    // security ourselves breaks its temporary (no-pairing) connections
    _hid_cfg.no_security = _mode_wii;
    // SInput asks the host to poll every slot pair (1.25 ms, Windows' own rate). Asking for the
    // report interval lets the radio poll coarsely, and each report then waits for the next poll.
    _hid_cfg.qos_latency_us = _mode_sinput ? 1250u : 0;

    if (_mode_wii)
    {
        // The Wii checks the HID record attributes, so serve a real remote's record verbatim. It
        // is registered first, so it gets the record handle it carries.
        const uint8_t *record = NULL;
        uint16_t record_len = 0;
        nwii_hid_get_sdp_record(&record, &record_len);
        _hid_cfg.sdp_record = record;
        _hid_cfg.sdp_record_len = record_len;
    }
    else
    {
        floss_sdp_hid_params_t p = {
            .service_name = _bt_hal_hid->name,
            .subclass = FLOSS_HID_SUBCLASS_GAMEPAD,
            .country_code = 33,
            .virtual_cable = true,
            .reconnect_initiate = true,
            .battery_power = false,
            .remote_wake = true,
            .normally_connectable = false,
            .boot_device = false,
            .supervision_timeout = 3200,
            .ssr_host_max_latency = 0xFFFF,
            .ssr_host_min_timeout = 0xFFFF,
            .report_descriptor = _bt_hal_hid->hid_report_descriptor,
            .report_descriptor_len = _bt_hal_hid->hid_report_descriptor_len,
        };
        _hid_cfg.sdp_record = _hid_record;
        _hid_cfg.sdp_record_len = floss_sdp_build_hid(_hid_record, sizeof(_hid_record), &p);
    }

    if (floss_hid_init(&_hid_cfg, &_bt_hal_hid_callbacks) != FLOSS_OK)
        return false;

    // Every mode lets the host take the master role. A Wii in particular hangs up on a remote that
    // refuses the role switch once other remotes are connected or a game is running. Sniff on a PC
    // (12.5 ms on Windows) caps SInput below its report rate.
    floss_gap_set_default_link_policy(_mode_sinput ? FLOSS_LINK_POLICY_ROLE_SWITCH
                                                   : (FLOSS_LINK_POLICY_ROLE_SWITCH | FLOSS_LINK_POLICY_SNIFF));

    if (_mode_wii)
    {
        floss_l2cap_set_unknown_cid_hook(_bt_hal_wii_unknown_cid);
        floss_timer_start(&_wii_stall_timer, BT_HAL_WII_STALL_CHECK_MS * 1000u);
    }
    return true;
}

static void _bt_hal_timers_init(void)
{
    floss_timer_init(&_report_timer, _bt_hal_report_tick, NULL);
    floss_timer_init(&_reconnect_timer, _bt_hal_reconnect_handler, NULL);
    floss_timer_init(&_wii_reconnect_timer, _bt_hal_wii_reconnect_handler, NULL);
    floss_timer_init(&_wii_page_stall_timer, _bt_hal_wii_page_stall_handler, NULL);
    floss_timer_init(&_wii_teardown_timer, _bt_hal_wii_teardown_handler, NULL);
    floss_timer_init(&_wii_stall_timer, _bt_hal_wii_stall_handler, NULL);
    floss_timer_init(&_probe_timer, _bt_hal_probe_timeout, NULL);
}

// (Re)start the stack with this address. A restart resets the controller, which drops every link
// at once: no callbacks run for them, so the session state is cleared here.
static bool _bt_hal_stack_start(const uint8_t *mac)
{
    _connected = false;
    _send_pending = false;
    _wii_acl_handle = BT_HAL_NO_HANDLE;
    _wii_teardown_handle = BT_HAL_NO_HANDLE;
    _wii_page_pending = false;
    _wii_stale_link = false;
    _probe_chan = NULL;
    _probe_handle = BT_HAL_NO_HANDLE;

    floss_init_t init = {
        .transport = floss_cyw43_transport(),
        .chipset = &floss_chipset_cyw43,
    };
    if (floss_init(&init) != FLOSS_OK)
        return false;

    // floss_init forgets every timer
    _bt_hal_timers_init();
#if HOJA_BT_FLOSS_DIAG
    _diag_max_slots = 0;
    floss_snoop_set_hook(_bt_hal_diag_snoop);
#endif

    floss_gap_security_t sec;
    floss_gap_security_defaults(&sec);
    sec.secure_connections = false; // Keys from USB pairing are P-192
    sec.pin_handler = _bt_hal_pin_handler;
    floss_gap_set_security(&sec);

    if (_bt_mode_ready && !_bt_hal_mode_apply())
        return false;

    // Listeners run in the order they were added. HID clears its session when the radio comes
    // up, so ours goes after it: a reconnect started on that event must not be cleared again.
    floss_gap_add_listener(_bt_hal_gap_event, NULL);

    floss_addr_t addr;
    _bt_hal_addr_from_hoja(mac, &addr);
    floss_hci_set_addr_override(&addr);
    memcpy(_radio_mac, mac, 6);

    _bt_hal_apply_scan();
    return floss_start() == FLOSS_OK;
}

// Restarts run from the task, never from inside the stack
static void _bt_hal_restart_as(const uint8_t *mac)
{
    memcpy(_restart_mac, mac, 6);
    _restart_pending = true;
    _connected = false;
    _send_pending = false;
}

// Brings the radio back with the current mode's address
static void _bt_hal_restart(void)
{
    _bt_hal_restart_as(core_current_params()->transport_dev_mac);
}

/***********************************************/
/********* Transport Defines *******************/

void transport_bt_stop()
{
}

bool transport_bt_init(core_params_s *params)
{
    _bt_hal_params = params;

    if (!_bt_hal_params || !_bt_hal_params->hid_device)
        return false;

    _bt_hal_hid = _bt_hal_params->hid_device;

    if (!_bt_stack_up)
    {
        if (!_floss_lock)
            _floss_lock = spin_lock_init((uint)spin_lock_claim_unused(true));

        // Radio firmware, the shared bus and its background worker
        while (cyw43_arch_init())
        {
            sys_hal_sleep_ms(1000);
        }
        _bt_stack_up = true;

        // Auto mode checks the saved hosts before any mode is set up
        if (!autodetect_bt_probe_pending())
            _bt_hal_mode_setup(params);

        return _bt_hal_stack_start(params->transport_dev_mac);
    }

    // Already running (Auto mode picked this mode): set it up and bring the radio back with it
    _bt_hal_mode_setup(params);
    _bt_hal_restart();
    return true;
}

void transport_bt_task(uint64_t timestamp)
{
    (void)timestamp;

#if HOJA_BT_FLOSS_DIAG
    uint32_t task_now = time_us_32();
    if (_diag_task_last_us && (task_now - _diag_task_last_us) > _diag_task_gap_max_us)
        _diag_task_gap_max_us = task_now - _diag_task_last_us;
    _diag_task_last_us = task_now;
    _diag_nested_us = 0;
#endif

    floss_poll();

#if HOJA_BT_FLOSS_DIAG
    uint32_t poll_us = (time_us_32() - task_now) - _diag_nested_us;
    if (poll_us > _diag_poll_max_us)
        _diag_poll_max_us = poll_us;
#endif

    if (_restart_pending)
    {
        _restart_pending = false;
        _bt_hal_stack_start(_restart_mac);
        return;
    }

    if (_wii_stale_link)
    {
        _wii_stale_link = false;
        if (_bt_hal_is_wii())
            _bt_hal_wii_radio_cycle();
        return;
    }

    // The host closed the HID channels; FLOSS closes the link after it
    if (_connected && floss_hid_state() != FLOSS_HID_STATE_CONNECTED)
        _bt_hal_session_closed();

    _bt_hal_try_send();

    // A shutdown asked for from a Bluetooth callback runs here, outside the stack
    if (_shutdown_pending)
    {
        _shutdown_pending = false;
        tp_evt_s pevt = {
            .evt = TP_EVT_POWERCOMMAND,
            .evt_powercommand = {.power_command = TP_POWERCOMMAND_SHUTDOWN}};
        transport_evt_cb(pevt);
        return;
    }

    bt_hal_inbound_report_s inbound;
    if (hoja_fifo_bt_inbound_pop(&_bt_inbound_fifo, &inbound))
    {
        core_report_tunnel_cb(inbound.data, inbound.len);
    }
}

/***********************************************/
/********* Static info *************************/

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
