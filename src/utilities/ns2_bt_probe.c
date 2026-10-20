#include "utilities/ns2_bt_probe.h"

#if defined(HOJA_NS2_PROBE_LOG)

#include <string.h>
#include "pico/time.h"
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

_Static_assert(sizeof(ns2p_log_s) <= 4096, "the probe log fits one sector");

static ns2p_log_s _log = {.magic = NS2P_MAGIC};
static uint32_t _first_ms = 0;

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
    // LE: a minute of whatever happens
    if (ms < 60000u && _log.count < NS2P_MAX)
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

    static uint8_t sector[4096];
    memset(sector, 0xFF, sizeof(sector));
    memcpy(sector, &_log, sizeof(_log));
    flash_hal_write_sector(BOOT_LAYOUT_STAGE_HEADER_OFFSET, sector, sizeof(sector), 100);
    sys_hal_bootloader();
}

#endif
