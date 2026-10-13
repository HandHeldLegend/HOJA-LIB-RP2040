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

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
#include "transport/transport_wlan.h"
#include "utilities/boot.h"
#include "devices/battery.h"
#include "usb/webusb.h"
#include "dongle.h"
#include "dongle_gamepad.h"
#include "dongle_link.h"
#endif

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

#define BT_STORE_IMAGE_BYTES     (((sizeof(bt_store_image_s) + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE)

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
// SInput: a PC polls the link every 1.25 ms once reports come this often, so they arrive evenly.
// A board's build may set another rate.
// SInput: three slot pairs (3.75 ms). Reports cross the air on Bluetooth's 1.25 ms grid, so a whole
// number of slot pairs keeps them evenly spaced (measured: 76% within 0.3 ms of nominal, 4 ms 46%)
#ifndef BT_HAL_SINPUT_INTERVAL_US
#define BT_HAL_SINPUT_INTERVAL_US 3750
#endif
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
// This session runs the dongle link (the link profile owns scanning)
static bool _mode_dongle = false;

static void _bt_hal_apply_scan(void)
{
    if (_mode_dongle)
        return;
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

// Radio bus transactions: the diagnostic build links with --wrap for the cyw43 driver's bus
// calls, which counts and times every transaction
static uint32_t _diag_bus_count = 0;
static uint32_t _diag_bus_us = 0;
static uint8_t _diag_send_txn = 0;      // Transactions in the last send
static uint16_t _diag_send_bus_us = 0;  // Their time
static uint8_t _diag_poll_txn_max = 0;  // Most transactions in one poll (without sends)
static uint32_t _diag_nested_txn = 0;   // Transactions of sends inside the current poll

#define BT_DIAG_BUS_BEGIN() uint32_t _bus_t0 = time_us_32()
#define BT_DIAG_BUS_END()   do { _diag_bus_us += time_us_32() - _bus_t0; _diag_bus_count++; } while (0)

int __real_cyw43_read_bytes(void *self, uint32_t fn, uint32_t addr, size_t len, uint8_t *buf);
int __wrap_cyw43_read_bytes(void *self, uint32_t fn, uint32_t addr, size_t len, uint8_t *buf)
{
    BT_DIAG_BUS_BEGIN();
    int r = __real_cyw43_read_bytes(self, fn, addr, len, buf);
    BT_DIAG_BUS_END();
    return r;
}

int __real_cyw43_write_bytes(void *self, uint32_t fn, uint32_t addr, size_t len, const uint8_t *src);
int __wrap_cyw43_write_bytes(void *self, uint32_t fn, uint32_t addr, size_t len, const uint8_t *src)
{
    BT_DIAG_BUS_BEGIN();
    int r = __real_cyw43_write_bytes(self, fn, addr, len, src);
    BT_DIAG_BUS_END();
    return r;
}

uint32_t __real_cyw43_read_reg_u32(void *self, uint32_t fn, uint32_t reg);
uint32_t __wrap_cyw43_read_reg_u32(void *self, uint32_t fn, uint32_t reg)
{
    BT_DIAG_BUS_BEGIN();
    uint32_t r = __real_cyw43_read_reg_u32(self, fn, reg);
    BT_DIAG_BUS_END();
    return r;
}

int __real_cyw43_read_reg_u16(void *self, uint32_t fn, uint32_t reg);
int __wrap_cyw43_read_reg_u16(void *self, uint32_t fn, uint32_t reg)
{
    BT_DIAG_BUS_BEGIN();
    int r = __real_cyw43_read_reg_u16(self, fn, reg);
    BT_DIAG_BUS_END();
    return r;
}

int __real_cyw43_read_reg_u8(void *self, uint32_t fn, uint32_t reg);
int __wrap_cyw43_read_reg_u8(void *self, uint32_t fn, uint32_t reg)
{
    BT_DIAG_BUS_BEGIN();
    int r = __real_cyw43_read_reg_u8(self, fn, reg);
    BT_DIAG_BUS_END();
    return r;
}

int __real_cyw43_write_reg_u32(void *self, uint32_t fn, uint32_t reg, uint32_t val);
int __wrap_cyw43_write_reg_u32(void *self, uint32_t fn, uint32_t reg, uint32_t val)
{
    BT_DIAG_BUS_BEGIN();
    int r = __real_cyw43_write_reg_u32(self, fn, reg, val);
    BT_DIAG_BUS_END();
    return r;
}

int __real_cyw43_write_reg_u16(void *self, uint32_t fn, uint32_t reg, uint16_t val);
int __wrap_cyw43_write_reg_u16(void *self, uint32_t fn, uint32_t reg, uint16_t val)
{
    BT_DIAG_BUS_BEGIN();
    int r = __real_cyw43_write_reg_u16(self, fn, reg, val);
    BT_DIAG_BUS_END();
    return r;
}

int __real_cyw43_write_reg_u8(void *self, uint32_t fn, uint32_t reg, uint32_t val);
int __wrap_cyw43_write_reg_u8(void *self, uint32_t fn, uint32_t reg, uint32_t val)
{
    BT_DIAG_BUS_BEGIN();
    int r = __real_cyw43_write_reg_u8(self, fn, reg, val);
    BT_DIAG_BUS_END();
    return r;
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

    // Touchpad bytes (unused on this board): radio bus transactions per send and per poll, the
    // last send's bus time, report timing, and the link's max slots
    const char *n1, *n2;
    uint32_t us1, us2;
    tasks_diag_take_worst(&n1, &us1, &n2, &us2);
    (void)n1;
    (void)us1;
    (void)n2;
    (void)us2;
    uint8_t *t = &data[35];
    t[0] = _diag_send_txn;
    t[1] = _diag_poll_txn_max;
    _bt_hal_diag_put_us(&t[2], _diag_send_bus_us);
    _diag_poll_txn_max = 0;
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
        _diag_step = (uint8_t)((_diag_step + 1) % FLOSS_ARRAY_SIZE(_diag_intervals_us));
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
        uint32_t send_txn0 = _diag_bus_count, send_bus0 = _diag_bus_us;
#endif
        floss_hid_send_input_now(report.data[0], &report.data[1], (uint16_t)(report.size - 1));
#if HOJA_BT_FLOSS_DIAG
        uint32_t send_txn = _diag_bus_count - send_txn0;
        _diag_send_txn = (uint8_t)(send_txn > 255u ? 255u : send_txn);
        _diag_send_bus_us = (uint16_t)(_diag_bus_us - send_bus0);
        _diag_nested_txn += send_txn;
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
    bool saved_pc = _bt_hal_is_stored_identity_valid(gamepad_config->host_mac_sinput);
    if (found == CORE_REPORTFORMAT_UNDEFINED && saved_pc)
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
    floss_err_t err = floss_l2cap_connect(evt->handle, FLOSS_PSM_SDP, &_probe_ops, NULL, FLOSS_SEC_NONE,
                                          &_probe_chan);
    if (err != FLOSS_OK)
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

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
static void _bt_hal_call_adv_rate(bool host_connected);
#endif

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
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    _bt_hal_call_adv_rate(true);
#endif
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
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    _bt_hal_call_adv_rate(false);
#endif

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

// The dongle: the WLAN dongle transport (transport_wlan_*) on a Bluetooth link instead. We connect
// to the dongle with FLOSS's link profile and open a second, reliable channel; HOJA-LIB-DONGLE
// carries the session, identity, mode following, status and config traffic over them (see
// dongle_link.h). The dongle stores nothing and pairs every time; we remember its address only.
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
static floss_link_config_t _link_cfg;
static bool _dongle_running = false;    // The dongle transport is up (it may be waiting for the link)
static floss_l2cap_chan_t *_dongle_rel = NULL;
static dongle_cfg_gamepad_s _dongle_cfg;

// The dongle's mode, picked up by hoja.c between task passes (transport_wlan_take_mode)
static volatile core_reportformat_t _dongle_mode_request = CORE_REPORTFORMAT_UNDEFINED;

// Config app commands arrive inside floss_poll; they run from the task (some write flash)
#define BT_HAL_DONGLE_CONFIG_RX_LEN 8
typedef struct
{
    uint16_t len;
    uint8_t data[64];
} bt_hal_dongle_config_rx_s;

static bt_hal_dongle_config_rx_s _dongle_config_rx[BT_HAL_DONGLE_CONFIG_RX_LEN];
static uint8_t _dongle_config_rx_head = 0;
static uint8_t _dongle_config_rx_tail = 0;

static dongle_mode_t _bt_hal_dongle_mode_from_format(core_reportformat_t fmt)
{
    switch (fmt)
    {
    case CORE_REPORTFORMAT_SWPRO:    return DONGLE_MODE_SWITCH;
    case CORE_REPORTFORMAT_SINPUT:   return DONGLE_MODE_SINPUT;
    case CORE_REPORTFORMAT_XINPUT:   return DONGLE_MODE_XINPUT;
    case CORE_REPORTFORMAT_SLIPPI:   return DONGLE_MODE_SLIPPI;
    case CORE_REPORTFORMAT_SNES:     return DONGLE_MODE_SNES;
    case CORE_REPORTFORMAT_N64:      return DONGLE_MODE_N64;
    case CORE_REPORTFORMAT_GAMECUBE: return DONGLE_MODE_GAMECUBE;
    default:                         return DONGLE_MODE_SWITCH;
    }
}

// Modes the dongle can present, for following it
static core_reportformat_t _bt_hal_dongle_format_from_mode(dongle_mode_t mode)
{
    switch (mode)
    {
    case DONGLE_MODE_SWITCH:   return CORE_REPORTFORMAT_SWPRO;
    case DONGLE_MODE_SINPUT:   return CORE_REPORTFORMAT_SINPUT;
    case DONGLE_MODE_XINPUT:   return CORE_REPORTFORMAT_XINPUT;
    case DONGLE_MODE_SLIPPI:   return CORE_REPORTFORMAT_SLIPPI;
    case DONGLE_MODE_SNES:     return CORE_REPORTFORMAT_SNES;
    case DONGLE_MODE_N64:      return CORE_REPORTFORMAT_N64;
    case DONGLE_MODE_GAMECUBE: return CORE_REPORTFORMAT_GAMECUBE;
    default:                   return CORE_REPORTFORMAT_UNDEFINED;
    }
}

static void _bt_hal_dongle_fill_cfg(core_params_s *params)
{
    const hoja_config_s *cfg = hoja_config_get();
    const char *dev_maker = (cfg && cfg->device_maker) ? cfg->device_maker : "HHL";
    const char *dev_name = (cfg && cfg->device_name) ? cfg->device_name : NULL;

    memset(&_dongle_cfg, 0, sizeof(_dongle_cfg));
    _dongle_cfg.mode = _bt_hal_dongle_mode_from_format(params->core_report_format);
    const boot_info_s *boot = boot_get_info();
    _dongle_cfg.mode_forced = boot && boot->mode_chosen;
    _dongle_cfg.pairing = (params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0;
    _dongle_cfg.fw_version = FIRMWARE_VERSION_TIMESTAMP;
    _dongle_cfg.evt.rumble = true;
    _dongle_cfg.evt.player_number = true;
    _dongle_cfg.evt.transport_status = true;

    dongle_wake_strcopy(_dongle_cfg.manufacturer, DONGLE_WAKE_MANUFACTURER_LEN, dev_maker);
    if (params->hid_device != NULL)
    {
        _dongle_cfg.vid = params->hid_device->vid;
        _dongle_cfg.pid = params->hid_device->pid;
        dongle_wake_strcopy(_dongle_cfg.name, DONGLE_WAKE_NAME_LEN, params->hid_device->name);
    }
    else if (cfg && (cfg->usb_vid || cfg->usb_pid))
    {
        _dongle_cfg.vid = cfg->usb_vid;
        _dongle_cfg.pid = cfg->usb_pid;
        if (dev_name)
            dongle_wake_strcopy(_dongle_cfg.name, DONGLE_WAKE_NAME_LEN, dev_name);
    }
}

// --- HOJA-LIB-DONGLE hooks ---

uint64_t dongle_api_hook_get_time_us_u64(void)
{
    return time_us_64();
}

uint16_t dongle_api_hook_get_rand_u16(void)
{
    return (uint16_t)(get_rand_32() & 0xFFFFu);
}

bool dongle_api_gamepad_hook_get_inputreport(uint8_t data[64], uint16_t *len, bool *reliable)
{
    core_report_s report = {0};
    if (!core_get_generated_report(&report))
        return false;

    uint16_t n = report.size > 64 ? 64 : report.size;
    memset(data, 0, 64);
    memcpy(data, report.data, n);
    *len = n;
    *reliable = report.reliable;
    return true;
}

void dongle_api_gamepad_hook_set_outputreport(const uint8_t data[64], uint16_t len)
{
    core_report_tunnel_cb(data, len);
}

void dongle_api_gamepad_hook_set_player(uint8_t player_number)
{
    tp_evt_s evt = {
        .evt = TP_EVT_PLAYERLED,
        .evt_playernumber = {.player_number = player_number},
    };
    transport_evt_cb(evt);
}

static void _bt_hal_dongle_notify_connection(bool connected)
{
    tp_evt_s evt = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {
            .connection = connected ? TP_CONNECTION_CONNECTED : TP_CONNECTION_DISCONNECTED,
        },
    };
    transport_evt_cb(evt);
}

void dongle_api_gamepad_hook_set_transport(bool connected)
{
    _bt_hal_dongle_notify_connection(connected);
}

void dongle_api_gamepad_hook_set_rumble(uint8_t left, uint8_t right, uint8_t left_brake, uint8_t right_brake)
{
    tp_evt_s evt = {
        .evt = TP_EVT_ERMRUMBLE,
        .evt_ermrumble = {.left = left, .right = right, .leftbrake = left_brake, .rightbrake = right_brake},
    };
    transport_evt_cb(evt);
}

void dongle_api_gamepad_hook_config_rx(const uint8_t data[64], uint16_t len)
{
    uint8_t next = (uint8_t)((_dongle_config_rx_head + 1u) % BT_HAL_DONGLE_CONFIG_RX_LEN);
    if (next == _dongle_config_rx_tail || len == 0 || len > 64)
        return;
    _dongle_config_rx[_dongle_config_rx_head].len = len;
    memcpy(_dongle_config_rx[_dongle_config_rx_head].data, data, len);
    _dongle_config_rx_head = next;
}

// The dongle lines our reports up with its console sends
void dongle_api_gamepad_hook_shift_reports(int16_t delta_us)
{
    floss_link_shift_phase(delta_us);
}

bool dongle_api_gamepad_hook_mode_request(dongle_mode_t mode)
{
    core_reportformat_t format = _bt_hal_dongle_format_from_mode(mode);
    if (format == CORE_REPORTFORMAT_UNDEFINED)
        return false;
    _dongle_mode_request = format;
    return true;
}

// --- The link ---

// The next live report: the newest input report, trimmed, ready for the tick to publish
static uint8_t _dongle_report[DONGLE_LINK_REPORT_MAX];
static uint16_t _dongle_report_len;
static bool _dongle_have_report = false;

static void _bt_hal_dongle_build(void)
{
    _dongle_have_report = dongle_api_gamepad_link_next_report(_dongle_report, &_dongle_report_len);
}

// Reports are built ahead of their send (building takes 0.2-0.4 ms on the GCU-2): the link's tick
// then only sends, on time, and every report leaves the same age. If the task loop missed that,
// the tick builds it, late.
#define BT_HAL_DONGLE_BUILD_LEAD_US 400
static bool _dongle_built = false;

static void _bt_hal_dongle_prebuild(void)
{
    uint32_t due;
    if (_dongle_built || !floss_link_next_report_at(&due))
        return;
    if ((int32_t)(time_us_32() - (due - BT_HAL_DONGLE_BUILD_LEAD_US)) < 0)
        return;
    _bt_hal_dongle_build();
    _dongle_built = true;
}

static bool _bt_hal_dongle_send_app(void);
static bool _dongle_last_tick_app = false;

// Each tick sends one packet, so nothing queues behind it in the radio (at one report per slot
// pair the air has no room for a second): the report built ahead (or now), or, in turn with it,
// one message of the config app's when one is waiting (a reply, or the app's live report). With
// no app traffic every tick is a report. The dongle's pacing request applies from the next period.
static void _bt_hal_dongle_tick(void)
{
    bool app = false;
    if (!_dongle_last_tick_app || !dongle_api_gamepad_adopted())
        app = _bt_hal_dongle_send_app();
    _dongle_last_tick_app = app;

    if (!app)
    {
        if (!_dongle_built)
            _bt_hal_dongle_build();
        if (_dongle_have_report)
        {
            floss_link_set_input(DONGLE_LINK_IN_REPORT, _dongle_report, (uint8_t)_dongle_report_len);
            tasks_mark_sent_isr();
        }
    }
    _dongle_built = false; // a report built for an app tick is dropped; the next one is fresh
    _dongle_have_report = false;

    uint16_t iv = dongle_api_gamepad_link_interval_us();
    if (iv)
        _link_cfg.report_interval_us = iv;
}

static void _bt_hal_dongle_rel_open(floss_l2cap_chan_t *ch, uint16_t status)
{
    if (ch != _dongle_rel)
        return;
    if (status != 0)
    {
        // No reliable channel, no session: drop the link, it connects again
        _dongle_rel = NULL;
        floss_gap_disconnect(ch->handle, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
        return;
    }
    dongle_api_gamepad_link_set_up(true);
}

static void _bt_hal_dongle_rel_close(floss_l2cap_chan_t *ch)
{
    if (ch != _dongle_rel)
        return;
    _dongle_rel = NULL;
    dongle_api_gamepad_link_set_up(false);
    _bt_hal_dongle_notify_connection(false);
}

static void _bt_hal_dongle_rel_data(floss_l2cap_chan_t *ch, const uint8_t *data, uint16_t len)
{
    if (ch == _dongle_rel)
        dongle_api_gamepad_link_rx_msg(data, len);
}

static const floss_l2cap_ops_t _bt_hal_dongle_rel_ops = {
    .on_open = _bt_hal_dongle_rel_open,
    .on_close = _bt_hal_dongle_rel_close,
    .on_data = _bt_hal_dongle_rel_data,
};

static void _bt_hal_dongle_connected(const floss_addr_t *peer)
{
    // Auto mode: a dongle answered, so this is the mode
    autodetect_dongle_found();

    const floss_conn_t *conn = floss_hci_conn_by_addr(peer);
    if (!conn || floss_l2cap_connect(conn->handle, DONGLE_LINK_PSM_RELIABLE, &_bt_hal_dongle_rel_ops, NULL,
                                     FLOSS_SEC_ENCRYPTED, &_dongle_rel) != FLOSS_OK)
    {
        _dongle_rel = NULL;
        if (conn)
            floss_gap_disconnect(conn->handle, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
    }
}

static void _bt_hal_dongle_disconnected(uint8_t reason)
{
    (void)reason; // The link connects again by itself
    if (_dongle_rel)
    {
        _dongle_rel = NULL;
        dongle_api_gamepad_link_set_up(false);
        _bt_hal_dongle_notify_connection(false);
    }
}

static void _bt_hal_dongle_output(uint8_t report_id, const uint8_t *data, uint8_t len)
{
    if (report_id == DONGLE_LINK_OUT_STATUS)
        dongle_api_gamepad_link_rx_status(data, len);
}

static const floss_link_callbacks_t _bt_hal_dongle_callbacks = {
    .on_connected = _bt_hal_dongle_connected,
    .on_disconnected = _bt_hal_dongle_disconnected,
    .on_output = _bt_hal_dongle_output,
    .on_report_tick = _bt_hal_dongle_tick,
};

// --- Link mode: the PC app (HHL Gamepad Link) over HID ---
//
// HHL Gamepad Link on a PC takes the dongle's place, over the PC's own Bluetooth, with the gamepad
// as a HID device of its own (dongle_link.h, "Link mode"). HID rather than a serial channel: a PC's
// radio keeps a HID link's timing (measured on an Intel radio: a serial (RFCOMM) link alone loses
// ~25 ms two or three times a second, with HID none), pairing is the OS's own, and every OS lets
// an app open a vendor-defined HID collection without a driver.
//
// Link mode has its own address (BT_HAL_APP_MAC_OFFSET), so a PC keeps it apart from the gamepad's
// Bluetooth modes. Dongle mode first calls the PC bonded to that address; if it doesn't connect,
// or no app answers in BT_HAL_APP_HOST_WAIT_MS, the radio restarts and the link looks for a
// dongle. Pairing mode: findable for Link for BT_HAL_APP_PAIR_MS, then the dongle search.
//
// The PC app can also pair Link mode from a Bluetooth mode (the call service, below): the radio
// alone restarts as Link mode's and pages that PC, which pairs it, then the gamepad restarts in
// Link mode. Measured on a GCU-2: paired in about a second this way, where pairing right after a
// restart into dongle mode took until about 20 s after boot.
typedef enum
{
    BT_HAL_APP_OFF = 0, // Not this run: the dongle link runs
    BT_HAL_APP_WAITING, // Calls the PC (or becomes findable) once the radio is up
    BT_HAL_APP_CALLING, // Opening HID to the bonded PC
    BT_HAL_APP_PAIRING, // Findable, for a PC to pair
    BT_HAL_APP_OPEN,    // HID up; running once the app has sent its host info
} bt_hal_app_state_t;

// Link mode's address: the gamepad's, last byte + this. A PC keeps a HID device's descriptor for
// its address, also across a new pairing: a new descriptor needs a new address.
#define BT_HAL_APP_MAC_OFFSET 0x21u

// Pages to the PC, in 0.625 ms slots; a PC listens for pages every 1.28 s. Called to Link by the
// app, the PC is there and wants us, but its radio may be busy for a moment (it just lost the
// gamepad's other connection): longer pages, and a few of them.
#define BT_HAL_APP_PAGE_TIMEOUT        0x1000 // 2.56 s
#define BT_HAL_APP_CALLED_PAGE_TIMEOUT 0x2000 // 5.12 s
#define BT_HAL_APP_CALLED_TRIES        3u

#define BT_HAL_APP_HOST_WAIT_MS  4000u  // HID up but no app: the dongle link takes over
#define BT_HAL_APP_SILENT_MS     3000u  // The app sends its host info every second: silent this long, gone
#define BT_HAL_APP_FIRST_WAIT_MS 30000u // Just paired: the PC installs the HID device first
#define BT_HAL_APP_SLOT_PAIR_US  1250u  // A PC polls us in slot pairs
#define BT_HAL_APP_QOS_US        BT_HAL_APP_SLOT_PAIR_US // Poll every slot pair, as SInput's HID asks

#define BT_HAL_APP_PAIR_MS            30000u // Pairing: findable for Link this long
#define BT_HAL_APP_PAIR_PAGES         3u     // Pages to the PC that asked (5.12 s each), then only findable
#define BT_HAL_APP_PAIR_AUTH_MS       8000u  // Linked to it and it hasn't paired: we start pairing
#define BT_HAL_APP_PAIR_AUTH_RETRY_US 2000u  // Our pairing request didn't fit the command queue
#define BT_HAL_APP_PAIR_SETTLE_MS     1000u  // It set up Link mode's HID device: a moment, then Link mode

static bt_hal_app_state_t _app_state = BT_HAL_APP_OFF;
static bool _app_skip = false;          // No PC app this time: the dongle link runs until the next start
static bool _app_host_seen = false;     // The app sent its host info
static bool _app_token_due = false;     // The call token goes to the app (DONGLE_LINK_MSG_TOKEN)
static bool _app_last_tick_msg = false; // Messages take turns with live reports
static uint8_t _app_called_tries = 0;   // Calls to the PC left, when the app called us to Link
static floss_timer_t _app_timer;        // Paces the reports
static floss_timer_t _app_wait_timer;   // Waits for the app, or for pairing
static uint32_t _app_deadline;          // The next report's tick
static uint16_t _app_seq;               // Live report sequence number
static char _app_name[48];              // "<gamepad> Link"

// Pairing from a Bluetooth mode, for the PC app that asked (the call service)
static bool _app_pair_here = false;        // The radio restarted as Link mode's to pair
static bool _app_pair_set_up = false;      // ...and that PC has set up Link mode's HID device
static floss_addr_t _app_pair_pc;          // The PC that asked
static uint8_t _app_pair_pages = 0;        // Pages to it left (0: not paging it)
static uint16_t _app_pair_handle;          // Its link, to pair on
static floss_timer_t _app_pair_auth_timer; // Starts pairing on it ourselves

static uint64_t _call_restart_at = 0; // Called to Link mode: the gamepad restarts then (0: not called)

static bool _bt_hal_call_token(uint8_t token[DONGLE_LINK_CALL_TOKEN_LEN]);

// Link mode's HID collection (dongle_link.h): live reports, messages in, messages out
static const uint8_t _app_descriptor[] = {
    0x06, 0x48, 0xFF,       // Usage page (vendor, DONGLE_LINK_APP_USAGE_PAGE)
    0x09, 0x4C,             // Usage (DONGLE_LINK_APP_USAGE)
    0xA1, 0x01,             // Collection (application)
    0x15, 0x00,             //   Logical minimum 0
    0x26, 0xFF, 0x00,       //   Logical maximum 255
    0x75, 0x08,             //   8 bits
    0x85, 0x01, 0x09, 0x01, //   Report 1: live
    0x95, DONGLE_LINK_APP_LIVE_LEN, 0x81, 0x02,
    0x85, 0x02, 0x09, 0x02, //   Report 2: messages in
    0x95, DONGLE_LINK_APP_MSG_LEN, 0x81, 0x02,
    0x85, 0x03, 0x09, 0x03, //   Report 3: messages out
    0x95, DONGLE_LINK_APP_MSG_LEN, 0x91, 0x02,
    0xC0,                   // End collection
};

static void _bt_hal_app_mac(uint8_t *mac)
{
    memcpy(mac, gamepad_config->gamepad_mac_address, 6);
    mac[5] = (uint8_t)(mac[5] + BT_HAL_APP_MAC_OFFSET);
}

// The PC Link mode paired with: a bond made under Link mode's address
static const floss_bond_t *_bt_hal_app_bond(void)
{
    uint8_t mac[6];
    floss_addr_t local;
    _bt_hal_app_mac(mac);
    _bt_hal_addr_from_hoja(mac, &local);
    const floss_bond_t *found = NULL;
    for (uint8_t i = 0; i < floss_gap_bond_count(); i++)
    {
        const floss_bond_t *b = floss_gap_bond_at(i);
        if (b && floss_addr_eq(&b->local, &local))
            found = b; // The newest one wins
    }
    return found;
}

static bool _bt_hal_app_wanted(void)
{
    return !_app_skip && (_pairing_mode || _bt_hal_app_bond() != NULL);
}

// The app went away (call it again) or didn't answer (the dongle link takes over). Either way the
// radio restarts, which drops the link.
static void _bt_hal_app_end(bool answered)
{
    floss_timer_stop(&_app_timer);
    floss_timer_stop(&_app_wait_timer);
    if (_app_state == BT_HAL_APP_OPEN)
    {
        dongle_api_gamepad_link_set_up(false);
        _bt_hal_dongle_notify_connection(false);
    }
    _app_state = BT_HAL_APP_OFF;
    _app_skip = !answered;
    _app_pair_here = false;
    _bt_hal_restart();
}

// [lane][length][payload], padded to the report's size
static bool _bt_hal_app_send_msg_report(uint8_t lane, const uint8_t *data, uint16_t len)
{
    uint8_t r[DONGLE_LINK_APP_MSG_LEN] = {0};
    if (len > DONGLE_LINK_MSG_MAX)
        return false;
    r[0] = lane;
    r[1] = (uint8_t)len;
    memcpy(&r[2], data, len);
    return floss_hid_send_input_now(DONGLE_LINK_APP_REPORT_MSG, r, sizeof(r)) == FLOSS_OK;
}

// One message for the app: the call token when it's due, else a reliable message (identity, reports
// that must arrive, config replies), else the config app's live report. False when there's none.
static bool _bt_hal_app_send_msg(void)
{
    uint8_t msg[DONGLE_LINK_MSG_MAX];
    uint16_t len;
    if (_app_token_due && dongle_api_gamepad_adopted())
    {
        msg[0] = DONGLE_LINK_MSG_TOKEN;
        if (!_bt_hal_call_token(&msg[1]))
        {
            _app_token_due = false;
            return false;
        }
        if (!_bt_hal_app_send_msg_report(DONGLE_LINK_APP_LANE_MSG, msg, 1u + DONGLE_LINK_CALL_TOKEN_LEN))
            return false;
        _app_token_due = false;
        return true;
    }
    if (dongle_api_gamepad_link_peek_msg(msg, &len))
    {
        if (!_bt_hal_app_send_msg_report(DONGLE_LINK_APP_LANE_MSG, msg, len))
            return false;
        dongle_api_gamepad_link_msg_sent();
        return true;
    }
    if (dongle_api_gamepad_link_take_bulk(msg, &len))
    {
        _bt_hal_app_send_msg_report(DONGLE_LINK_APP_LANE_BULK, msg, len);
        return true;
    }
    return false;
}

// The app's report interval (dongle_link_host_s), to the nearest whole slot pair. Off that grid,
// sends miss the PC's polls. Measured on Windows: 4 ms gave 146 reports/s with an 11 ms median
// delay, 3.75 ms 254/s with 0.4 ms; 2 ms gave 444/s at 1.0 ms, 2.5 ms 372/s at 0.4 ms.
static uint32_t _bt_hal_app_interval_us(void)
{
    uint32_t asked = dongle_api_gamepad_link_interval_us();
    if (!asked)
        asked = DONGLE_LINK_INTERVAL_DEFAULT_US;
    uint32_t pairs = (asked + BT_HAL_APP_SLOT_PAIR_US / 2u) / BT_HAL_APP_SLOT_PAIR_US;
    return (pairs ? pairs : 1u) * BT_HAL_APP_SLOT_PAIR_US;
}

// Paced as the app asks: one report per tick, a message in turn with the live report, as on the
// dongle link. Nothing goes while the radio still holds earlier reports (HID's in-flight cap): the
// newest is sent at the next tick instead.
static void _bt_hal_app_tick(floss_timer_t *t, void *arg)
{
    (void)arg;
    uint32_t iv = _bt_hal_app_interval_us();
    uint32_t now = floss_hal_time_us();
    _app_deadline += iv;
    if (floss_time_diff(now, _app_deadline) >= 0)
        _app_deadline = now + iv; // A whole interval behind: from now, no burst
    floss_timer_start_at(t, _app_deadline);

    if (_app_state != BT_HAL_APP_OPEN || !floss_hid_can_send_now())
        return;

    bool msg = false;
    if (!_app_last_tick_msg || !dongle_api_gamepad_adopted())
        msg = _bt_hal_app_send_msg();
    _app_last_tick_msg = msg;
    if (msg)
        return;

    uint8_t trimmed[DONGLE_LINK_REPORT_MAX];
    uint16_t n;
    uint32_t sampled = time_us_32();
    if (!dongle_api_gamepad_link_next_report(trimmed, &n))
        return;
    // [seq u16][sampled_us u32][full length][bytes]: the trimmed zeros stay zero
    uint8_t r[DONGLE_LINK_APP_LIVE_LEN] = {0};
    floss_put_le16(&r[0], _app_seq);
    floss_put_le32(&r[2], sampled);
    r[6] = trimmed[0];
    memcpy(&r[7], &trimmed[1], (size_t)(n - 1u));
    if (floss_hid_send_input_now(DONGLE_LINK_APP_REPORT_LIVE, r, sizeof(r)) == FLOSS_OK)
    {
        _app_seq++;
        tasks_mark_sent_isr();
    }
}

// HID is up but no app said hello (the PC is on, the app isn't running), the app went quiet (it
// quit: the PC keeps HID up without it), or pairing timed out. The dongle link takes over (or,
// pairing from a Bluetooth mode, that mode comes back). Paired from one: on to Link mode.
static void _bt_hal_app_wait_expired(floss_timer_t *t, void *arg)
{
    (void)t;
    (void)arg;
    if (_app_pair_here && _app_pair_set_up)
        _call_restart_at = time_us_64();
    else if (_app_state == BT_HAL_APP_OPEN || _app_state == BT_HAL_APP_PAIRING)
        _bt_hal_app_end(false);
}

static void _bt_hal_app_on_connected(const floss_addr_t *host)
{
    (void)host;
    // Paired from a Bluetooth mode, and the PC has set up Link mode's HID: on to Link mode proper
    if (_app_pair_here)
    {
        _app_pair_set_up = true;
        floss_timer_start(&_app_wait_timer, BT_HAL_APP_PAIR_SETTLE_MS * 1000u);
        return;
    }
    const bool just_paired = (_app_state == BT_HAL_APP_PAIRING);
    const uint32_t wait_ms = just_paired ? BT_HAL_APP_FIRST_WAIT_MS : BT_HAL_APP_HOST_WAIT_MS;
    _app_state = BT_HAL_APP_OPEN;
    _app_host_seen = false;
    _app_last_tick_msg = false;
    floss_gap_set_scan(false, false);
    dongle_api_gamepad_link_set_up(true);
    floss_timer_start(&_app_wait_timer, wait_ms * 1000u);
    _app_deadline = floss_hal_time_us();
    floss_timer_start_at(&_app_timer, _app_deadline + _bt_hal_app_interval_us());
}

static void _bt_hal_app_on_disconnected(uint8_t reason)
{
    (void)reason;
    if (_app_state == BT_HAL_APP_OPEN)
        _bt_hal_app_end(_app_host_seen); // An app that was running is called again
}

static void _bt_hal_app_on_connect_failed(uint8_t reason, uint16_t channel_status)
{
    (void)reason;
    (void)channel_status;
    if (_app_state != BT_HAL_APP_CALLING)
        return;
    if (_app_called_tries)
    {
        // Called by the app: call the PC again (the radio restarts, so from the top)
        _app_state = BT_HAL_APP_OFF;
        _bt_hal_restart();
        return;
    }
    _bt_hal_app_end(false);
}

// The app's reports: messages (its host info, outputs, config) and status
static void _bt_hal_app_on_output(uint8_t report_id, const uint8_t *data, uint16_t len)
{
    if (report_id != DONGLE_LINK_APP_REPORT_OUT || len < 2u || data[1] > len - 2u)
        return;
    if (_app_host_seen)
        floss_timer_start(&_app_wait_timer, BT_HAL_APP_SILENT_MS * 1000u); // The app is there
    const uint8_t *p = &data[2];
    if (data[0] == DONGLE_LINK_APP_LANE_MSG && data[1])
    {
        if (p[0] == DONGLE_LINK_MSG_HOST && !_app_host_seen)
        {
            _app_host_seen = true;
            _app_token_due = true;
            floss_timer_start(&_app_wait_timer, BT_HAL_APP_SILENT_MS * 1000u);
            autodetect_dongle_found(); // Auto mode: the app counts as a dongle
        }
        dongle_api_gamepad_link_rx_msg(p, data[1]);
    }
    else if (data[0] == DONGLE_LINK_APP_LANE_STATUS)
        dongle_api_gamepad_link_rx_status(p, data[1]);
}

static const floss_hid_callbacks_t _bt_hal_app_callbacks = {
    .on_connected = _bt_hal_app_on_connected,
    .on_disconnected = _bt_hal_app_on_disconnected,
    .on_output_report = _bt_hal_app_on_output,
    .on_connect_failed = _bt_hal_app_on_connect_failed,
};

// Link mode's HID device, registered before the radio starts
static bool _bt_hal_app_hid_init(void)
{
    const char *name = _bt_hal_hid ? _bt_hal_hid->name : "HOJA Gamepad";
    snprintf(_app_name, sizeof(_app_name), "%s Link", name);

    floss_hid_config_defaults(&_hid_cfg);
    _hid_cfg.name = _app_name;
    _hid_cfg.report_descriptor = _app_descriptor;
    _hid_cfg.report_descriptor_len = sizeof(_app_descriptor);
    _hid_cfg.subclass = 0;
    _hid_cfg.class_of_device = FLOSS_COD_GAMEPAD;
    _hid_cfg.vendor_id_source = 2; // USB
    _hid_cfg.vendor_id = _bt_hal_hid ? _bt_hal_hid->vid : 0;
    _hid_cfg.product_id = _bt_hal_hid ? _bt_hal_hid->pid : 0;
    _hid_cfg.version = 0x0100;
    _hid_cfg.auto_discoverable = false;
    _hid_cfg.auto_reconnect = false;
    _hid_cfg.app_controls_scan = true;
    _hid_cfg.qos_latency_us = BT_HAL_APP_QOS_US;
    floss_sdp_hid_params_t sp = {
        .service_name = _app_name,
        .subclass = 0,
        .country_code = 33,
        .virtual_cable = true,
        .reconnect_initiate = true,
        .battery_power = false,
        .remote_wake = false,
        .normally_connectable = false,
        .boot_device = false,
        .supervision_timeout = 3200,
        .ssr_host_max_latency = 0xFFFF,
        .ssr_host_min_timeout = 0xFFFF,
        .report_descriptor = _app_descriptor,
        .report_descriptor_len = sizeof(_app_descriptor),
    };
    _hid_cfg.sdp_record = _hid_record;
    _hid_cfg.sdp_record_len = floss_sdp_build_hid(_hid_record, sizeof(_hid_record), &sp);
    if (floss_hid_init(&_hid_cfg, &_bt_hal_app_callbacks) != FLOSS_OK)
        return false;
    floss_gap_set_default_link_policy(FLOSS_LINK_POLICY_ROLE_SWITCH); // No sniff: it caps the rate
    return true;
}

// Asked to pair by the app on a PC: page that PC rather than wait for it to find us (an inquiry
// takes 5 s a round). The app there pairs on the link, then turns on Link mode's HID, and the PC
// connects it.
static void _bt_hal_app_pair_page(void)
{
    floss_gap_bond_delete(&_app_pair_pc); // The PC dropped its side before asking: pair afresh
    _app_pair_pages = BT_HAL_APP_PAIR_PAGES;
    _bt_hal_write_page_timeout(BT_HAL_APP_CALLED_PAGE_TIMEOUT);
    floss_gap_connect(&_app_pair_pc);
}

// The PC hasn't paired on our link: we start pairing ourselves (an app that only waits for us)
static void _bt_hal_app_pair_auth(floss_timer_t *t, void *arg)
{
    (void)arg;
    if (_app_state != BT_HAL_APP_PAIRING)
        return;
    if (floss_gap_authenticate(_app_pair_handle) != FLOSS_OK)
        floss_timer_start(t, BT_HAL_APP_PAIR_AUTH_RETRY_US);
}

// Paging the PC that asked to pair: a page that times out goes again. Once the link is up the
// app there pairs on it; we only start pairing if it hasn't in BT_HAL_APP_PAIR_AUTH_MS.
static void _bt_hal_app_pair_page_event(const floss_gap_evt_t *evt)
{
    if (_app_state != BT_HAL_APP_PAIRING)
        return;
    if (evt->type == FLOSS_GAP_EVT_PAIRING_COMPLETE || evt->type == FLOSS_GAP_EVT_PAIRING_FAILED ||
        (evt->type == FLOSS_GAP_EVT_DISCONNECTED && evt->handle == _app_pair_handle))
    {
        floss_timer_stop(&_app_pair_auth_timer);
        return;
    }
    if (!_app_pair_pages || !floss_addr_eq(&evt->addr, &_app_pair_pc))
        return;
    if (evt->type == FLOSS_GAP_EVT_CONNECTED)
    {
        _app_pair_pages = 0;
        _app_pair_handle = evt->handle;
        floss_timer_start(&_app_pair_auth_timer, BT_HAL_APP_PAIR_AUTH_MS * 1000u);
    }
    else if (evt->type == FLOSS_GAP_EVT_CONNECT_FAILED && --_app_pair_pages)
        floss_gap_connect(&_app_pair_pc);
}

// The radio is up: as Link mode's address, call the PC it paired with, or be findable to pair
static void _bt_hal_app_call(void)
{
    uint8_t mac[6];
    _bt_hal_app_mac(mac);
    if (memcmp(_radio_mac, mac, 6))
    {
        _bt_hal_restart_as(mac); // Back here once the radio is up as that address
        return;
    }
    if (_pairing_mode || _app_pair_here)
    {
        _app_state = BT_HAL_APP_PAIRING;
        floss_gap_set_scan(true, true); // Findable (dongle mode leaves scanning to us)
        floss_timer_start(&_app_wait_timer, BT_HAL_APP_PAIR_MS * 1000u);
        if (_app_pair_here)
            _bt_hal_app_pair_page();
        return;
    }
    const floss_bond_t *pc = _bt_hal_app_bond();
    if (!pc)
    {
        _bt_hal_app_end(false);
        return;
    }
    if (_app_called_tries)
        _app_called_tries--;
    const bool called = _app_called_tries || boot_get_info()->link_called;
    _bt_hal_write_page_timeout(called ? BT_HAL_APP_CALLED_PAGE_TIMEOUT : BT_HAL_APP_PAGE_TIMEOUT);
    _app_state = BT_HAL_APP_CALLING;
    if (floss_hid_connect(&pc->addr) != FLOSS_OK)
        _bt_hal_app_end(false);
}

// --- Calling it to Link (dongle_link.h): LE advertising and the call service ---
//
// In the direct Bluetooth modes the gamepad advertises so the PC app can see it wherever it is
// connected: call it over to Link with the token it was given, or pair it (while it isn't in use).

#define BT_HAL_CALL_STORE_KEY     (FLOSS_STORE_KEY_APP_BASE + 1u) // The call token
#define BT_HAL_CALL_PAIR_LEN      7u      // [DONGLE_LINK_CALL_PAIR][PC address, 6]
#define BT_HAL_CALL_INFO_MAX      96u     // The info characteristic's value, at most
#define BT_HAL_CALL_NAME_MAX      20u     // The gamepad's name in the scan response, at most
#define BT_HAL_CALL_ANSWER_US     150000u // A write's answer goes out before we act on it
#define BT_HAL_AD_SERVICE_DATA128 0x21    // AD type: service data, 128-bit UUID

// Advertising: every 1 s while a host is connected (measured: costs its link nothing), every 100 ms
// while not (no link to disturb), so the PC app finds the gamepad in about a second. 0.625 ms units.
#define BT_HAL_CALL_ADV_CONNECTED 1600u
#define BT_HAL_CALL_ADV_IDLE      160u

static uint64_t _pair_close_at = 0;   // Asked to pair: the LE link closes then (0: not asked)...
static uint64_t _pair_restart_at = 0; // ...and the radio restarts as Link mode's then
static uint16_t _pair_le_conn;        // The LE link the request came on

// UUIDs little-endian, as GATT and AD structures carry them
static const uint8_t _call_uuid[16] = {0x16, 0x01, 0x49, 0xEF, 0x17, 0xC6, 0xE8, 0x91,
                                       0x05, 0x45, 0x54, 0x6D, 0xE1, 0xDF, 0x16, 0x2A};
static const uint8_t _call_cmd_uuid[16] = {0xCB, 0xB7, 0x21, 0x58, 0x6B, 0x4E, 0x27, 0xA9,
                                           0x71, 0x45, 0x18, 0xEA, 0x7A, 0x11, 0x10, 0xA2};
static const uint8_t _call_info_uuid[16] = {0x8C, 0x0A, 0x7A, 0x2D, 0xBD, 0x19, 0xCD, 0xBF,
                                            0x79, 0x40, 0x6B, 0x88, 0x62, 0xF0, 0x64, 0xC8};

// The info characteristic: what the advertisement and scan response say, in one read
static uint16_t _bt_hal_call_info(uint8_t *out, uint16_t cap)
{
    uint8_t buf[3 + 6 + 1 + DONGLE_WAKE_NAME_LEN + 1 + sizeof(user_config->user_name)];
    uint16_t n = 0;
    buf[n++] = DONGLE_LINK_CALL_VERSION;
    buf[n++] = (uint8_t)core_current_reportformat();
    buf[n++] = _bt_hal_app_bond() ? DONGLE_LINK_CALL_F_LINKED : 0u;
    _bt_hal_app_mac(&buf[n]);
    n += 6;
    uint8_t len = (uint8_t)strnlen(_bt_hal_hid ? _bt_hal_hid->name : "", DONGLE_WAKE_NAME_LEN);
    buf[n++] = len;
    memcpy(&buf[n], _bt_hal_hid ? _bt_hal_hid->name : "", len);
    n += len;
    len = (uint8_t)strnlen((const char *)user_config->user_name, sizeof(user_config->user_name));
    buf[n++] = len;
    memcpy(&buf[n], user_config->user_name, len);
    n += len;
    if (n > cap)
        n = cap;
    memcpy(out, buf, n);
    return n;
}

static int _bt_hal_call_info_read(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr,
                                  uint16_t offset, uint8_t *out, uint16_t cap)
{
    (void)conn;
    (void)attr_handle;
    (void)attr;
    uint8_t info[BT_HAL_CALL_INFO_MAX];
    uint16_t n = _bt_hal_call_info(info, sizeof(info));
    if (offset > n)
        return -FLOSS_ATT_ERR_INVALID_OFFSET;
    uint16_t take = (uint16_t)(n - offset);
    if (take > cap)
        take = cap;
    memcpy(out, &info[offset], take);
    return take;
}

// The gamepad's call token, made the first time it is needed and kept
static bool _bt_hal_call_token(uint8_t token[DONGLE_LINK_CALL_TOKEN_LEN])
{
    const int len = DONGLE_LINK_CALL_TOKEN_LEN;
    if (floss_hal_store_read(BT_HAL_CALL_STORE_KEY, token, len) == len)
        return true;
    for (unsigned i = 0; i < DONGLE_LINK_CALL_TOKEN_LEN; i += 4)
    {
        uint32_t r = get_rand_32();
        memcpy(&token[i], &r, 4);
    }
    return floss_hal_store_write(BT_HAL_CALL_STORE_KEY, token, DONGLE_LINK_CALL_TOKEN_LEN) == FLOSS_OK;
}

static uint8_t _bt_hal_call_write(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr,
                                  uint16_t offset, const uint8_t *data, uint16_t len, uint8_t flags)
{
    (void)attr_handle;
    (void)attr;
    (void)flags;
    uint8_t token[DONGLE_LINK_CALL_TOKEN_LEN];
    if (offset == 0 && len == BT_HAL_CALL_PAIR_LEN && data[0] == DONGLE_LINK_CALL_PAIR)
    {
        // Not while a host is connected: nobody nearby takes a gamepad out of a game
        if (_connected)
            return FLOSS_ATT_ERR_WRITE_NOT_PERMITTED;
        _bt_hal_addr_from_hoja(&data[1], &_app_pair_pc);
        _pair_le_conn = conn;
        _pair_close_at = time_us_64() + BT_HAL_CALL_ANSWER_US;
        _pair_restart_at = _pair_close_at + BT_HAL_CALL_ANSWER_US; // The close goes out too
        return 0;
    }
    if (offset != 0 || len != 1u + DONGLE_LINK_CALL_TOKEN_LEN || data[0] != DONGLE_LINK_CALL_TO_LINK)
        return FLOSS_ATT_ERR_VALUE_NOT_ALLOWED;
    if (!_bt_hal_call_token(token) || memcmp(token, &data[1], sizeof(token)))
        return FLOSS_ATT_ERR_WRITE_NOT_PERMITTED;
    _call_restart_at = time_us_64() + BT_HAL_CALL_ANSWER_US;
    return 0;
}

static const floss_gatt_attr_t _call_service[] = {
    FLOSS_GATT_PRIMARY_SERVICE128(_call_uuid),
    FLOSS_GATT_CHARACTERISTIC(FLOSS_GATT_PROP_WRITE),
    {.uuid128 = _call_cmd_uuid, .perms = FLOSS_GATT_P_WRITE, .write = _bt_hal_call_write,
     .max_len = 1u + DONGLE_LINK_CALL_TOKEN_LEN},
    FLOSS_GATT_CHARACTERISTIC(FLOSS_GATT_PROP_READ),
    {.uuid128 = _call_info_uuid, .perms = FLOSS_GATT_P_READ, .read = _bt_hal_call_info_read,
     .max_len = BT_HAL_CALL_INFO_MAX},
};

static void _bt_hal_call_adv_rate(bool host_connected)
{
    if (_mode_dongle || _app_pair_here)
        return;
    floss_le_adv_params_t p;
    floss_le_adv_params_defaults(&p);
    p.interval_min = host_connected ? BT_HAL_CALL_ADV_CONNECTED : BT_HAL_CALL_ADV_IDLE;
    p.interval_max = p.interval_min;
    floss_le_adv_set_params(&p);
}

// Direct Bluetooth modes: the call service and advertising. Before the radio starts (the GATT table
// is fixed once it runs); advertising starts once it is up.
static void _bt_hal_call_setup(void)
{
    if (_mode_dongle || _app_pair_here)
        return;
    floss_gatt_register(_call_service, FLOSS_ARRAY_SIZE(_call_service));

    _bt_hal_call_adv_rate(false);

    // Flags, then service data: [uuid][version][mode][flags][Link address as shown]
    uint8_t mac[6];
    _bt_hal_app_mac(mac);
    uint8_t sd[16 + 3 + 6];
    memcpy(sd, _call_uuid, 16);
    sd[16] = DONGLE_LINK_CALL_VERSION;
    sd[17] = (uint8_t)core_current_reportformat();
    sd[18] = _bt_hal_app_bond() ? DONGLE_LINK_CALL_F_LINKED : 0u;
    memcpy(&sd[19], mac, 6);
    floss_le_ad_t ad;
    floss_le_ad_init(&ad);
    floss_le_ad_flags(&ad, FLOSS_AD_FLAG_GENERAL_DISC | FLOSS_AD_FLAG_NO_BREDR);
    floss_le_ad_add(&ad, BT_HAL_AD_SERVICE_DATA128, sd, sizeof(sd));
    floss_le_adv_set_data(ad.data, ad.len);

    // Scan response: the gamepad's name, and the user's from the config app (manufacturer data,
    // company 0xFFFF, as much as fits)
    floss_le_ad_t rsp;
    floss_le_ad_init(&rsp);
    uint8_t name_len = (uint8_t)strnlen(_bt_hal_hid->name, BT_HAL_CALL_NAME_MAX);
    floss_le_ad_add(&rsp, FLOSS_AD_NAME_COMPLETE, (const uint8_t *)_bt_hal_hid->name, name_len);
    uint8_t user[2 + sizeof(user_config->user_name)] = {0xFF, 0xFF};
    uint8_t user_len = (uint8_t)strnlen((const char *)user_config->user_name, sizeof(user_config->user_name));
    uint8_t room = (uint8_t)(FLOSS_LE_ADV_DATA_MAX - rsp.len - 2u - 2u); // AD header, company
    if (user_len > room)
        user_len = room;
    if (user_len)
    {
        memcpy(&user[2], user_config->user_name, user_len);
        floss_le_ad_add(&rsp, FLOSS_AD_MANUFACTURER, user, (uint8_t)(2u + user_len));
    }
    floss_le_adv_set_scan_rsp(rsp.data, rsp.len);
    floss_le_adv_start();
}

// Asked to pair: the LE link is closed (so the PC isn't left holding a dead one), then the radio
// restarts as Link mode's, to pair with the PC that asked
static void _bt_hal_pair_task(void)
{
    uint64_t now = time_us_64();
    if (_pair_close_at && now >= _pair_close_at)
    {
        _pair_close_at = 0;
        floss_le_disconnect(_pair_le_conn, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
    }
    if (!_pair_restart_at || now < _pair_restart_at)
        return;
    _pair_restart_at = 0;
    uint8_t mac[6];
    _bt_hal_app_mac(mac);
    _app_pair_here = true;
    _app_pair_set_up = false;
    _bt_hal_restart_as(mac);
}

// Called to Link mode: restart in dongle mode, in the same mode, where Link mode calls the PC
static void _bt_hal_call_task(void)
{
    _bt_hal_pair_task();
    if (!_call_restart_at || time_us_64() < _call_restart_at)
        return;
    boot_memory_s mem = {
        .report_format = (uint8_t)core_current_reportformat(),
        .gamepad_method = (uint8_t)GAMEPAD_METHOD_WLAN,
        .link_called = 1,
    };
    boot_set_memory(&mem);
    sys_hal_reboot();
}

static bool _bt_hal_dongle_link_up(void)
{
    return _dongle_rel != NULL || _app_state == BT_HAL_APP_OPEN;
}

static bool _bt_hal_dongle_apply(void)
{
    // Called to Link by the app: a few calls to the PC before the dongle search, once per boot
    static bool called_armed = false;
    if (!called_armed)
    {
        called_armed = true;
        const boot_info_s *boot = boot_get_info();
        if (boot && boot->link_called)
            _app_called_tries = BT_HAL_APP_CALLED_TRIES;
    }
    if (_bt_hal_app_wanted())
    {
        _app_state = BT_HAL_APP_WAITING; // Link mode first, once the radio is up
        return _bt_hal_app_hid_init();
    }
    floss_link_config_defaults(&_link_cfg, FLOSS_LINK_ROLE_PAD, FLOSS_LINK_CLASSIC);
    _link_cfg.name = _bt_hal_hid ? _bt_hal_hid->name : "HOJA Gamepad";
    _link_cfg.peer_name = DONGLE_LINK_NAME;
    _link_cfg.classic_pad_connects = true; // We find the dongle and run the link
    _link_cfg.classic_store_keys = false;  // It pairs every time; we remember its address only
    _link_cfg.report_interval_us = DONGLE_LINK_INTERVAL_DEFAULT_US;
    // The radio reports delivered packets two at a time, 3-8 ms late: room for that many, so
    // reports go to the radio one per period and are never held back and sent in pairs. The dongle
    // sees a report waiting in our radio (after a resend) and moves our clock a period on.
    _link_cfg.max_inflight = 4;
    // A dongle that is on answers a page within ~160 ms: give up on the remembered one quickly and
    // search for any other (Auto mode only looks for a dongle for a few seconds at power-up)
    _link_cfg.classic_page_timeout_ms = 1280;
    return floss_link_init(&_link_cfg, &_bt_hal_dongle_callbacks) == FLOSS_OK;
}

// One app packet for this tick: a reliable message (our identity, reports that must arrive, config
// replies) first, else the app's live report. False when there is none (or no room).
static bool _bt_hal_dongle_send_app(void)
{
    uint8_t msg[DONGLE_LINK_MSG_MAX];
    uint16_t len;
    if (_dongle_rel && _dongle_rel->state == FLOSS_L2CAP_OPEN && dongle_api_gamepad_link_peek_msg(msg, &len))
    {
        if (floss_l2cap_send(_dongle_rel, msg, len) != FLOSS_OK)
            return false;
        dongle_api_gamepad_link_msg_sent();
        return true;
    }
    if (dongle_api_gamepad_link_take_bulk(msg, &len))
    {
        floss_link_set_input(DONGLE_LINK_IN_BULK, msg, (uint8_t)len);
        return true;
    }
    return false;
}

// Config replies queue in the dongle library; a big block may fill it, so keep the radio going
#define BT_HAL_DONGLE_CONFIG_SEND_WAIT_US 200000

static bool _bt_hal_dongle_sink_send(const uint8_t *data, uint16_t size)
{
    uint64_t until = time_us_64() + BT_HAL_DONGLE_CONFIG_SEND_WAIT_US;
    while (!dongle_api_gamepad_config_send(data, size))
    {
        if (time_us_64() > until || !_bt_hal_dongle_link_up())
            return false;
        floss_poll(); // the link's ticks send the queued replies
    }
    return true;
}

static bool _bt_hal_dongle_sink_send_input(const uint8_t *data, uint16_t size)
{
    dongle_api_gamepad_bulk_send(data, size);
    return true;
}

static bool _bt_hal_dongle_sink_ready(int timeout_ms)
{
    (void)timeout_ms;
    return dongle_api_gamepad_adopted();
}

static const webusb_sink_s _bt_hal_dongle_webusb_sink = {
    .send = _bt_hal_dongle_sink_send,
    .send_input = _bt_hal_dongle_sink_send_input,
    .ready = _bt_hal_dongle_sink_ready,
};

static void _bt_hal_dongle_task(uint64_t timestamp)
{
    // The app's next command once the replies to the last one are out: answering never waits for
    // room (that would stall the task loop, and the inputs with it)
    while (_dongle_config_rx_tail != _dongle_config_rx_head && !dongle_api_gamepad_config_pending())
    {
        bt_hal_dongle_config_rx_s *rx = &_dongle_config_rx[_dongle_config_rx_tail];
        webusb_command_handler(rx->data, rx->len);
        _dongle_config_rx_tail = (uint8_t)((_dongle_config_rx_tail + 1u) % BT_HAL_DONGLE_CONFIG_RX_LEN);
    }
    webusb_send_rawinput(timestamp);
}
#endif

static void _bt_hal_on_ready(void)
{
    _wii_radio_cycling = false;

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    // Link mode first (the PC app), when one is saved or being paired
    if (_app_state == BT_HAL_APP_WAITING)
    {
        _bt_hal_app_call();
        return;
    }
#endif
    // Dongle mode: the link pages the dongle it remembers, else searches (pairing mode: any dongle)
    if (_mode_dongle)
    {
        if (_pairing_mode)
            floss_link_pair();
        return;
    }

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
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    _bt_hal_app_pair_page_event(evt);
#endif

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
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
        // The link only connects to dongles: Auto mode stops looking for hosts once one answers,
        // before the pairing and channels that follow
        if (_mode_dongle)
            autodetect_dongle_found();
#endif
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
    _report_interval_us = _mode_wii      ? BT_HAL_WII_POLLING_RATE_MS * 1000u
                          : _mode_sinput ? BT_HAL_SINPUT_INTERVAL_US
                                         : BT_HAL_TARGET_POLLING_RATE_MS * 1000u;
    _pairing_mode = (params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0;
    _bt_mode_ready = true;
}

static bool _bt_hal_mode_apply(void)
{
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    if (_app_pair_here)
    {
        _app_state = BT_HAL_APP_WAITING;
        return _bt_hal_app_hid_init();
    }
    if (_mode_dongle)
        return _bt_hal_dongle_apply();
#endif
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
    uint16_t policy = FLOSS_LINK_POLICY_ROLE_SWITCH;
    if (!_mode_sinput)
        policy |= FLOSS_LINK_POLICY_SNIFF;
    floss_gap_set_default_link_policy(policy);

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
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    floss_timer_init(&_app_timer, _bt_hal_app_tick, NULL);
    floss_timer_init(&_app_wait_timer, _bt_hal_app_wait_expired, NULL);
    floss_timer_init(&_app_pair_auth_timer, _bt_hal_app_pair_auth, NULL);
#endif
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
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    _app_state = BT_HAL_APP_OFF;
#endif

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
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    if (_bt_mode_ready)
        _bt_hal_call_setup();
#endif

    // Listeners run in the order they were added. HID clears its session when the radio comes
    // up, so ours goes after it: a reconnect started on that event must not be cleared again.
    floss_gap_add_listener(_bt_hal_gap_event, NULL);

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    // Link mode goes first: up as its address straight away, not in a second start
    uint8_t app_mac[6];
    if (_app_state == BT_HAL_APP_WAITING)
    {
        _bt_hal_app_mac(app_mac);
        mac = app_mac;
    }
#endif
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

// Bring the radio up for this transport's mode, or back with it when already running (Auto mode
// moving on, or the dongle giving way to a host). Auto mode checks the saved hosts before any mode
// is set up.
static bool _bt_hal_stack_bringup(core_params_s *params)
{
    _bt_hal_params = params;

    if (!_bt_hal_params || !_bt_hal_params->hid_device)
        return false;

    _bt_hal_hid = _bt_hal_params->hid_device;

    if (autodetect_bt_probe_pending() && !_mode_dongle)
        _bt_mode_ready = false;
    else
        _bt_hal_mode_setup(params);

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
        return _bt_hal_stack_start(params->transport_dev_mac);
    }

    _bt_hal_restart();
    return true;
}

bool transport_bt_init(core_params_s *params)
{
    _mode_dongle = false;
    return _bt_hal_stack_bringup(params);
}

// The stack's share of the task loop, for both transports
static void _bt_hal_task(uint64_t timestamp)
{
    (void)timestamp;

#if HOJA_BT_FLOSS_DIAG
    uint32_t task_now = time_us_32();
    if (_diag_task_last_us && (task_now - _diag_task_last_us) > _diag_task_gap_max_us)
        _diag_task_gap_max_us = task_now - _diag_task_last_us;
    _diag_task_last_us = task_now;
    _diag_nested_us = 0;
    _diag_nested_txn = 0;
    uint32_t poll_txn0 = _diag_bus_count;
#endif

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    if (_mode_dongle)
        _bt_hal_dongle_prebuild();
#endif

    floss_poll();

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    // The next report's send is a deadline for the task loop (see tasks_set_deadline)
    uint32_t due;
    if (_mode_dongle && _app_state == BT_HAL_APP_OPEN)
        tasks_set_deadline(sys_hal_now_us() + (uint64_t)(int64_t)(int32_t)(_app_deadline - time_us_32()));
    else if (_mode_dongle && floss_link_next_report_at(&due))
        tasks_set_deadline(sys_hal_now_us() + (uint64_t)(int64_t)(int32_t)(due - time_us_32()));
    else
        tasks_set_deadline(0);
#endif

#if HOJA_BT_FLOSS_DIAG
    uint32_t poll_us = (time_us_32() - task_now) - _diag_nested_us;
    if (poll_us > _diag_poll_max_us)
        _diag_poll_max_us = poll_us;
    uint32_t poll_txn = (_diag_bus_count - poll_txn0) - _diag_nested_txn;
    if (poll_txn > _diag_poll_txn_max)
        _diag_poll_txn_max = (uint8_t)(poll_txn > 255u ? 255u : poll_txn);
#endif

    if (_restart_pending)
    {
        _restart_pending = false;
        _bt_hal_stack_start(_restart_mac);
        return;
    }
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    _bt_hal_call_task();
#endif

    if (_wii_stale_link)
    {
        _wii_stale_link = false;
        if (_bt_hal_is_wii())
            _bt_hal_wii_radio_cycle();
        return;
    }

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
    if (_mode_dongle)
    {
        _bt_hal_dongle_task(timestamp);
        return;
    }
#endif

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

void transport_bt_task(uint64_t timestamp)
{
    _bt_hal_task(timestamp);
}

/***********************************************/
/********* Dongle (transport_wlan_*) ***********/

#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
void transport_wlan_stop(void)
{
    // The radio stays up for whatever comes next (Auto mode moving on to a host restarts it)
    webusb_set_sink(NULL);
    if (_dongle_rel || _app_state == BT_HAL_APP_OPEN)
    {
        _dongle_rel = NULL;
        dongle_api_gamepad_link_set_up(false);
    }
    floss_timer_stop(&_app_timer);
    floss_timer_stop(&_app_wait_timer);
    _app_state = BT_HAL_APP_OFF;
    _app_skip = false;
    _bt_hal_dongle_notify_connection(false);
    _dongle_running = false;
    _mode_dongle = false;
    _dongle_mode_request = CORE_REPORTFORMAT_UNDEFINED;
}

bool transport_wlan_init(core_params_s *params)
{
    _bt_hal_dongle_fill_cfg(params);

    // Already up: the mode changed to follow the dongle, so only our identity changes
    if (_dongle_running)
    {
        _bt_hal_params = params;
        _bt_hal_hid = params->hid_device;
        dongle_api_gamepad_set_identity(_dongle_cfg.mode, _dongle_cfg.vid, _dongle_cfg.pid, _dongle_cfg.name,
                                        _dongle_cfg.manufacturer);
        return true;
    }

    dongle_api_gamepad_link_init(&_dongle_cfg);
    _dongle_rel = NULL;
    _dongle_mode_request = CORE_REPORTFORMAT_UNDEFINED;
    _dongle_config_rx_head = 0;
    _dongle_config_rx_tail = 0;
    _mode_dongle = true;
    if (!_bt_hal_stack_bringup(params))
    {
        _mode_dongle = false;
        return false;
    }
    webusb_set_sink(&_bt_hal_dongle_webusb_sink);
    _dongle_running = true;
    return true;
}

void transport_wlan_task(uint64_t timestamp)
{
    if (_dongle_running)
        _bt_hal_task(timestamp);
}

bool transport_wlan_take_mode(core_reportformat_t *format)
{
    if (!_dongle_running || _dongle_mode_request == CORE_REPORTFORMAT_UNDEFINED)
        return false;
    *format = _dongle_mode_request;
    _dongle_mode_request = CORE_REPORTFORMAT_UNDEFINED;
    return true;
}

bool transport_wlan_choosing(void)
{
    return _dongle_running && !_dongle_cfg.mode_forced && !dongle_api_gamepad_adopted();
}

uint8_t transport_wlan_static_supported(void)
{
    return 1;
}

uint8_t transport_wlan_static_part_status(void)
{
    return transport_bt_static_part_status();
}
#endif

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
