#include "utilities/ns2_bt_probe.h"

#if defined(HOJA_NS2_PROBE_LOG)

#include <string.h>
#include "pico/time.h"
#include "hardware/regs/addressmap.h"
#include "hal/flash_hal.h"
#include "hal/sys_hal.h"
#include "hal/rp2040/boot_layout.h"

#define NS2P_MAGIC     0x50324E53u // "SN2P"
#define NS2P_DATA      34
#define NS2P_MAX       100
// After the console's first report, this long for the rest of its conversation
#define NS2P_AFTER_MS  20000
#define NS2P_GIVEUP_MS 90000

typedef struct
{
    uint32_t ms;
    uint8_t kind;
    uint8_t len; // The report's full length (only the first NS2P_DATA bytes are kept)
    uint8_t data[NS2P_DATA];
} ns2p_entry_s;

typedef struct
{
    uint32_t magic;
    uint32_t count;
    ns2p_entry_s entries[NS2P_MAX];
} ns2p_log_s;

// A USB pairing kept for the LE test, in the sector's last bytes (a flush keeps it)
#define NS2P_PAIRING_MAGIC 0x4B50324Eu // "N2PK"
#define NS2P_PAIRING_AT    (4096 - 32)

typedef struct
{
    uint32_t magic;
    uint8_t host_mac[6];
    uint8_t ltk[16];
} ns2p_pairing_s;

_Static_assert(sizeof(ns2p_log_s) <= NS2P_PAIRING_AT, "the probe log fits before the pairing");
_Static_assert(NS2P_PAIRING_AT + sizeof(ns2p_pairing_s) <= 4096, "the pairing fits the sector");

static ns2p_log_s _log = {.magic = NS2P_MAGIC};
static uint32_t _first_ms = 0;
static uint8_t _sector[4096];

static const uint8_t *_ns2p_flash(void)
{
    return (const uint8_t *)(XIP_BASE + BOOT_LAYOUT_STAGE_HEADER_OFFSET);
}

// The Pro Controller 2's reports as its USB form has them (the NSO GameCube controller's, captured,
// with its ids): 0x05 (63 bytes in), 0x09 (buttons, sticks, then 52 bytes in), 0x02 (63 bytes out)
static const uint8_t _ns2p_report_descriptor[97] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x05, 0x05, 0xFF, 0x09, 0x01, 0x15, 0x00, 0x26, 0xFF,
    0x00, 0x95, 0x3F, 0x75, 0x08, 0x81, 0x02, 0x85, 0x09, 0x09, 0x01, 0x95, 0x02, 0x81, 0x02, 0x05,
    0x09, 0x19, 0x01, 0x29, 0x15, 0x25, 0x01, 0x95, 0x15, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75,
    0x03, 0x81, 0x03, 0x05, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09,
    0x35, 0x26, 0xFF, 0x0F, 0x95, 0x04, 0x75, 0x0C, 0x81, 0x02, 0xC0, 0x05, 0xFF, 0x09, 0x02, 0x26,
    0xFF, 0x00, 0x95, 0x34, 0x75, 0x08, 0x81, 0x02, 0x85, 0x02, 0x09, 0x01, 0x95, 0x3F, 0x91, 0x02,
    0xC0,
};

#if defined(HOJA_NS2_BT_PROBE) && HOJA_NS2_BT_PROBE
void ns2_bt_probe_identity(core_hid_device_t *device)
{
    device->pid = 0x2069;
    device->hid_report_descriptor = _ns2p_report_descriptor;
    device->hid_report_descriptor_len = sizeof(_ns2p_report_descriptor);
}
#endif

void ns2_bt_probe_record(ns2_bt_probe_kind_t kind, const uint8_t *data, uint16_t len)
{
    const uint32_t ms = to_ms_since_boot(get_absolute_time());
    if (!_first_ms)
        _first_ms = ms ? ms : 1;
    if (_log.count >= NS2P_MAX)
        return;

    ns2p_entry_s *e = &_log.entries[_log.count++];
    e->ms = ms;
    e->kind = (uint8_t)kind;
    e->len = (uint8_t)(len > 255 ? 255 : len);
    memset(e->data, 0, sizeof(e->data));
    if (data)
        memcpy(e->data, data, len < NS2P_DATA ? len : NS2P_DATA);
}

void ns2_bt_probe_task(uint64_t now_us)
{
    const uint32_t ms = (uint32_t)(now_us / 1000);
#if defined(HOJA_NS2_BLE_PROBE) && HOJA_NS2_BLE_PROBE
    // LE: three minutes of whatever happens (the console may wait out a 40 s procedure timeout).
    // Nothing recorded (USB, say): nothing to keep
    if (!_first_ms || (ms < 180000u && _log.count < NS2P_MAX))
        return;
#else
    bool talked = _first_ms && (ms - _first_ms) >= NS2P_AFTER_MS;
    if (!talked && _log.count < NS2P_MAX && ms < NS2P_GIVEUP_MS)
        return;
#endif
    ns2_bt_probe_flush();
}

void ns2_bt_probe_flush(void)
{
    static bool done = false;
    if (done)
        return;
    done = true;

    memset(_sector, 0xFF, sizeof(_sector));
    memcpy(_sector, &_log, sizeof(_log));
    memcpy(&_sector[NS2P_PAIRING_AT], &_ns2p_flash()[NS2P_PAIRING_AT], sizeof(ns2p_pairing_s));
    flash_hal_write_sector(BOOT_LAYOUT_STAGE_HEADER_OFFSET, _sector, sizeof(_sector), 100);
    sys_hal_bootloader();
}

#if defined(HOJA_NS2_BLE_PROBE) && HOJA_NS2_BLE_PROBE && defined(HOJA_NS2)

#include "floss/floss.h"
#include "cores/core_ns2gc.h"
#include "ns2_lib.h"

void ns2_bt_probe_pairing_save(const uint8_t host_mac[6], const uint8_t ltk[16])
{
    ns2p_pairing_s p = {.magic = NS2P_PAIRING_MAGIC};
    memcpy(p.host_mac, host_mac, 6);
    memcpy(p.ltk, ltk, 16);

    memcpy(_sector, _ns2p_flash(), sizeof(_sector));
    memcpy(&_sector[NS2P_PAIRING_AT], &p, sizeof(p));
    flash_hal_write_sector(BOOT_LAYOUT_STAGE_HEADER_OFFSET, _sector, sizeof(_sector), 100);
}

bool ns2_bt_probe_pairing_load(uint8_t host_mac[6], uint8_t ltk[16])
{
    ns2p_pairing_s p;
    memcpy(&p, &_ns2p_flash()[NS2P_PAIRING_AT], sizeof(p));
    if (p.magic != NS2P_PAIRING_MAGIC)
        return false;
    memcpy(host_mac, p.host_mac, 6);
    memcpy(ltk, p.ltk, 16);
    return true;
}

/***********************************************/
/********* The paired controller on LE *********/

#define NS2P_ATTR_MAX   64
#define NS2P_CCCD_MAX   8
#define NS2P_INPUT_US   8000

static floss_gatt_attr_t _ns2p_attrs[NS2P_ATTR_MAX];
static floss_gatt_cccd_t _ns2p_cccd[NS2P_CCCD_MAX];
static uint8_t _ns2p_ltk[16]; // MSB first
static uint16_t _ns2p_conn = 0xFFFFu;
static uint32_t _ns2p_input_at = 0;

// A reply the stack had no room for yet
static uint8_t _ns2p_reply[NS2_NOTIFY_MAX_SIZE];
static uint16_t _ns2p_reply_handle = 0;
static uint16_t _ns2p_reply_len = 0;

static int _ns2p_read(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr, uint16_t offset,
                      uint8_t *out, uint16_t cap)
{
    (void)conn;
    (void)attr;
    uint8_t r[4] = {(uint8_t)attr_handle, (uint8_t)(attr_handle >> 8), (uint8_t)offset, (uint8_t)(offset >> 8)};
    ns2_bt_probe_record(NS2P_ATT_READ, r, sizeof(r));
    return ns2_api_ble_read(attr_handle, offset, out, cap);
}

static uint8_t _ns2p_write(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr, uint16_t offset,
                           const uint8_t *data, uint16_t len, uint8_t flags)
{
    (void)conn;
    (void)attr;
    (void)offset;
    (void)flags;
    uint8_t w[NS2P_DATA];
    w[0] = (uint8_t)attr_handle;
    w[1] = (uint8_t)(attr_handle >> 8);
    uint16_t n = len < sizeof(w) - 2 ? len : sizeof(w) - 2;
    memcpy(&w[2], data, n);
    ns2_bt_probe_record(NS2P_ATT_WRITE, w, (uint16_t)(len + 2));
    ns2_api_ble_write(attr_handle, data, len);
    return 0;
}

// The console never runs SMP: it encrypts with the key from the pairing commands (EDIV and Rand 0)
static bool _ns2p_ltk_provide(uint16_t handle, uint16_t ediv, const uint8_t rand[8], uint8_t ltk[16], void *user)
{
    (void)handle;
    (void)rand;
    (void)user;
    uint8_t note[3] = {4, (uint8_t)ediv, (uint8_t)(ediv >> 8)};
    ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
    for (int i = 0; i < 16; i++)
        ltk[i] = _ns2p_ltk[15 - i];
    return true;
}

// NS2-LIB-HID's table as FLOSS attributes. Both number from handle 1 with one attribute per handle,
// so the handles come out as the retail controller's (checked against the table's own)
static uint16_t _ns2p_build_gatt(void)
{
    uint16_t count = 0;
    const ns2_gatt_attribute_s *t = ns2_gatt_get_table(&count);
    uint16_t n = 0;
    uint8_t cccds = 0;

    for (uint16_t i = 0; i < count; i++)
    {
        const ns2_gatt_attribute_s *e = &t[i];
        if (e->handle != n + 1 || n + 2 > NS2P_ATTR_MAX)
            return 0;
        floss_gatt_attr_t *a = &_ns2p_attrs[n];
        memset(a, 0, sizeof(*a));

        switch (e->kind)
        {
        case NS2_ATT_PRIMARY_SERVICE:
            a->type = FLOSS_GATT_UUID_PRIMARY_SERVICE;
            a->perms = FLOSS_GATT_P_READ;
            if (e->uuid128)
            {
                a->value = (void *)e->uuid128;
                a->max_len = 16;
            }
            else
                a->aux = e->uuid16;
            n++;
            break;

        case NS2_ATT_CHARACTERISTIC:
        {
            a->type = FLOSS_GATT_UUID_CHARACTERISTIC;
            a->perms = FLOSS_GATT_P_READ;
            a->aux = e->properties;
            floss_gatt_attr_t *v = &_ns2p_attrs[n + 1];
            memset(v, 0, sizeof(*v));
            v->type = e->uuid16;
            v->uuid128 = e->uuid128;
            if (e->value)
            {
                v->perms = FLOSS_GATT_P_READ;
                v->value = (void *)e->value;
                v->max_len = e->value_len;
            }
            else
            {
                v->perms = FLOSS_GATT_P_RW;
                v->read = _ns2p_read;
                v->write = _ns2p_write;
            }
            n += 2;
            break;
        }

        case NS2_ATT_CCC:
            if (cccds >= NS2P_CCCD_MAX)
                return 0;
            a->type = FLOSS_GATT_UUID_CCCD;
            a->perms = FLOSS_GATT_P_RW;
            a->value = &_ns2p_cccd[cccds++];
            a->max_len = 2;
            a->write = _ns2p_write;
            n++;
            break;

        case NS2_ATT_DESCRIPTOR:
        default:
            a->uuid128 = e->uuid128;
            a->type = e->uuid16;
            a->perms = FLOSS_GATT_P_RW;
            a->read = _ns2p_read;
            a->write = _ns2p_write;
            n++;
            break;
        }
    }
    return n;
}

uint8_t ns2_bt_probe_reconnect_setup(const uint8_t dev_mac[6], uint8_t adv[31])
{
    uint8_t host_mac[6];
    if (!ns2_bt_probe_pairing_load(host_mac, _ns2p_ltk))
        return 0;

    // The identity the USB pairing gave the console, now over BLE
    ns2_device_config_s cfg = {0};
    core_ns2gc_identity(&cfg, dev_mac);
    cfg.transport = NS2_TRANSPORT_BLE;
    memcpy(cfg.host_mac, host_mac, 6);
    uint8_t note[8] = {5, (uint8_t)ns2_api_init(&cfg)};
    ns2_api_set_ltk(_ns2p_ltk);

    floss_gatt_clear();
    memset(_ns2p_cccd, 0, sizeof(_ns2p_cccd));
    uint16_t n = _ns2p_build_gatt();
    note[2] = (uint8_t)n;
    note[3] = n ? (uint8_t)floss_gatt_register(_ns2p_attrs, n) : 0;
    note[4] = (uint8_t)floss_gatt_last_handle();
    floss_smp_set_ltk_provider(_ns2p_ltk_provide, NULL);

    // Waking flag set, as a paired controller asks its console to take it back
    uint8_t len = ns2_api_get_advertisement(adv, true);
    note[5] = len;
    note[6] = host_mac[4];
    note[7] = host_mac[5];
    ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
    return len;
}

void ns2_bt_probe_le_event(const floss_le_evt_t *evt)
{
    if (evt->type == FLOSS_LE_EVT_CONNECTED)
    {
        uint8_t mac[6];
        for (int i = 0; i < 6; i++)
            mac[i] = evt->addr.b[5 - i];
        _ns2p_conn = evt->handle;
        _ns2p_reply_len = 0;
        ns2_api_ble_connected(mac);
    }
    else if (evt->type == FLOSS_LE_EVT_DISCONNECTED && evt->handle == _ns2p_conn)
    {
        _ns2p_conn = 0xFFFFu;
        ns2_api_ble_disconnected();
    }
}

#if defined(HOJA_NS2_CYW_SCAN) && HOJA_NS2_CYW_SCAN

#define NS2P_HCI_LE_FEATURES 0x2003u
#define NS2P_HCI_WRITE_RAM   0xFC4Cu
#define NS2P_HCI_READ_RAM    0xFC4Du
#define NS2P_SCAN_CHUNK      240u
#define NS2P_SCAN_SKIP       0x10000u // Past an unreadable stretch
#define NS2P_SCAN_HITS       16

typedef struct
{
    uint32_t start;
    uint32_t size;
} ns2p_range_s;

// The Broadcom radios' usual map: ROM from 0, RAM from 0x200000
static const ns2p_range_s _ns2p_ranges[] = {
    {0x00000000u, 0x00100000u},
    {0x00200000u, 0x00080000u},
};

typedef enum
{
    NS2P_SCAN_OFF,
    NS2P_SCAN_FEATURES,
    NS2P_SCAN_LENGTHS,
    NS2P_SCAN_READ,
    NS2P_SCAN_WRITE,
    NS2P_SCAN_VERIFY,
    NS2P_SCAN_DONE,
} ns2p_scan_state_t;

static ns2p_scan_state_t _scan_state = NS2P_SCAN_OFF;
static bool _scan_waiting = false;
static uint8_t _scan_range = 0;
static uint32_t _scan_addr = 0;
static bool _scan_range_logged = false;
static uint8_t _scan_features[8];
static uint8_t _scan_buf[7 + NS2P_SCAN_CHUNK];
static uint8_t _scan_tail = 0; // Bytes carried over from the last chunk
static uint32_t _scan_hits[NS2P_SCAN_HITS];
static uint8_t _scan_hit_count = 0;
static uint8_t _scan_write = 0;
static uint8_t _scan_errors = 0;
static uint8_t _scan_chunk = 0;

// Read lengths to try first, at the start of RAM: the largest the radio takes is the scan's step
static const uint8_t _scan_lengths[] = {1, 4, 8, 16, 32, 64, 128, 200, 240};
static uint8_t _scan_length_i = 0;

static void _ns2p_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void _ns2p_scan_next_range(void)
{
    _scan_range++;
    _scan_tail = 0;
    _scan_range_logged = false;
    if (_scan_range < sizeof(_ns2p_ranges) / sizeof(_ns2p_ranges[0]))
        _scan_addr = _ns2p_ranges[_scan_range].start;
    else
    {
        // Every hit, then patch the ones in RAM
        for (uint8_t i = 0; i < _scan_hit_count; i++)
        {
            uint8_t note[5] = {10};
            _ns2p_put32(&note[1], _scan_hits[i]);
            ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
        }
        _scan_write = 0;
        _scan_state = NS2P_SCAN_WRITE;
    }
}

static void _ns2p_scan_cb(void *ctx, uint16_t opcode, uint8_t status, const uint8_t *ret, uint16_t ret_len)
{
    (void)ctx;
    _scan_waiting = false;

    if (opcode == NS2P_HCI_LE_FEATURES)
    {
        uint8_t note[10] = {_scan_state == NS2P_SCAN_FEATURES ? 7 : 11, status};
        if (status == 0 && ret_len >= 8)
        {
            memcpy(&note[2], ret, 8);
            if (_scan_state == NS2P_SCAN_FEATURES)
                memcpy(_scan_features, ret, 8);
        }
        ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
        if (_scan_state == NS2P_SCAN_FEATURES)
        {
            _scan_range = 0;
            _scan_addr = _ns2p_ranges[0].start;
            _scan_length_i = 0;
            _scan_state = (status == 0) ? NS2P_SCAN_LENGTHS : NS2P_SCAN_DONE;
        }
        else
            _scan_state = NS2P_SCAN_DONE;
        return;
    }

    if (opcode == NS2P_HCI_WRITE_RAM)
    {
        uint8_t note[6] = {12, status};
        _ns2p_put32(&note[2], _scan_hits[_scan_write]);
        ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
        _scan_write++;
        return;
    }

    if (_scan_state == NS2P_SCAN_LENGTHS)
    {
        uint8_t len = _scan_lengths[_scan_length_i];
        uint8_t note[4 + 8] = {14, len, status, (uint8_t)ret_len};
        if (ret)
            memcpy(&note[4], ret, ret_len < 8 ? ret_len : 8);
        ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
        if (status == 0 && ret_len)
            _scan_chunk = len;
        if (++_scan_length_i >= sizeof(_scan_lengths))
            _scan_state = _scan_chunk ? NS2P_SCAN_READ : NS2P_SCAN_DONE;
        return;
    }

    // Read_RAM
    const ns2p_range_s *r = &_ns2p_ranges[_scan_range];
    if (status != 0 || ret_len == 0)
    {
        if (_scan_errors < 12)
        {
            _scan_errors++;
            uint8_t note[6] = {8, status};
            _ns2p_put32(&note[2], _scan_addr);
            ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
        }
        _scan_addr = (_scan_addr + NS2P_SCAN_SKIP) & ~(NS2P_SCAN_SKIP - 1u);
        _scan_tail = 0;
    }
    else
    {
        if (!_scan_range_logged)
        {
            // The first bytes each range gives, to see the command works
            _scan_range_logged = true;
            uint8_t note[5 + 16] = {9};
            _ns2p_put32(&note[1], _scan_addr);
            memcpy(&note[5], ret, ret_len < 16 ? ret_len : 16);
            ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
        }
        uint16_t n = ret_len < _scan_chunk ? ret_len : _scan_chunk;
        memcpy(&_scan_buf[_scan_tail], ret, n);
        uint16_t total = (uint16_t)(_scan_tail + n);
        for (uint16_t i = 0; i + 8 <= total; i++)
        {
            if (memcmp(&_scan_buf[i], _scan_features, 8) == 0 && _scan_hit_count < NS2P_SCAN_HITS)
                _scan_hits[_scan_hit_count++] = _scan_addr - _scan_tail + i;
        }
        uint8_t keep = total < 7 ? (uint8_t)total : 7;
        memmove(_scan_buf, &_scan_buf[total - keep], keep);
        _scan_tail = keep;
        _scan_addr += n;
    }

    if (_scan_addr >= r->start + r->size)
        _ns2p_scan_next_range();
}

void ns2_bt_probe_cyw_scan_start(void)
{
    _scan_state = NS2P_SCAN_FEATURES;
    _scan_waiting = false;
    uint8_t note[1] = {13};
    ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
}

static void _ns2p_scan_task(void)
{
    if (_scan_state == NS2P_SCAN_OFF || _scan_waiting)
        return;

    switch (_scan_state)
    {
    case NS2P_SCAN_FEATURES:
    case NS2P_SCAN_VERIFY:
        if (floss_hci_cmd(NS2P_HCI_LE_FEATURES, NULL, 0, _ns2p_scan_cb, NULL) == FLOSS_OK)
            _scan_waiting = true;
        break;

    case NS2P_SCAN_LENGTHS:
    {
        uint8_t p[5];
        _ns2p_put32(p, 0x00200000u);
        p[4] = _scan_lengths[_scan_length_i];
        if (floss_hci_cmd(NS2P_HCI_READ_RAM, p, sizeof(p), _ns2p_scan_cb, NULL) == FLOSS_OK)
            _scan_waiting = true;
        break;
    }

    case NS2P_SCAN_READ:
    {
        uint32_t end = _ns2p_ranges[_scan_range].start + _ns2p_ranges[_scan_range].size;
        uint32_t left = end - _scan_addr;
        uint8_t p[5];
        _ns2p_put32(p, _scan_addr);
        p[4] = (uint8_t)(left < _scan_chunk ? left : _scan_chunk);
        if (floss_hci_cmd(NS2P_HCI_READ_RAM, p, sizeof(p), _ns2p_scan_cb, NULL) == FLOSS_OK)
            _scan_waiting = true;
        break;
    }

    case NS2P_SCAN_WRITE:
    {
        // The 2M PHY is bit 8: the mask's second byte, bit 0. Only in RAM
        while (_scan_write < _scan_hit_count && _scan_hits[_scan_write] < 0x00200000u)
            _scan_write++;
        if (_scan_write >= _scan_hit_count)
        {
            _scan_state = NS2P_SCAN_VERIFY;
            break;
        }
        uint8_t p[5];
        _ns2p_put32(p, _scan_hits[_scan_write] + 1u);
        p[4] = (uint8_t)(_scan_features[1] | 0x01u);
        if (floss_hci_cmd(NS2P_HCI_WRITE_RAM, p, sizeof(p), _ns2p_scan_cb, NULL) == FLOSS_OK)
            _scan_waiting = true;
        break;
    }

    case NS2P_SCAN_DONE:
        _scan_state = NS2P_SCAN_OFF;
        ns2_bt_probe_flush();
        break;

    default:
        break;
    }
}
#endif

void ns2_bt_probe_ble_task(void)
{
#if defined(HOJA_NS2_CYW_SCAN) && HOJA_NS2_CYW_SCAN
    _ns2p_scan_task();
#endif
    if (_ns2p_conn == 0xFFFFu)
        return;

    // Command replies first, in order: one held back until the stack has room
    for (;;)
    {
        if (!_ns2p_reply_len && !ns2_api_ble_get_notification(&_ns2p_reply_handle, _ns2p_reply, &_ns2p_reply_len))
            break;
        floss_err_t err = floss_gatt_notify(_ns2p_conn, _ns2p_reply_handle, _ns2p_reply, _ns2p_reply_len);
        if (err == FLOSS_ERR_BUSY || err == FLOSS_ERR_NOMEM)
            return;
        if (err != FLOSS_OK)
        {
            uint8_t note[4] = {6, (uint8_t)err, (uint8_t)_ns2p_reply_handle, (uint8_t)_ns2p_reply[0]};
            ns2_bt_probe_record(NS2P_NOTE, note, sizeof(note));
        }
        _ns2p_reply_len = 0;
    }

    uint32_t now = time_us_32();
    if ((int32_t)(now - _ns2p_input_at) < 0)
        return;
    _ns2p_input_at = now + NS2P_INPUT_US;
    uint8_t report[NS2_INPUT_REPORT_SIZE];
    if (ns2_api_generate_inputreport(report))
        floss_gatt_notify(_ns2p_conn, NS2_HANDLE_PRO2_INPUT_VALUE, report, sizeof(report));
}

#endif

#endif
