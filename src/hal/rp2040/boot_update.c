#include "hal/rp2040/boot_update.h"

#include <string.h>

#if defined(HOJA_BOOTLOADER)

#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "hal/rp2040/boot_layout.h"
#include "hal/sys_hal.h"

// The reply leaves before the restart
#define BOOT_UPDATE_RESTART_US 300000u
#define BOOT_UPDATE_NO_GAUGE   0xFFu

// The end of this firmware in flash (the linker's)
extern char __flash_binary_end;

typedef enum
{
    BOOT_UPDATE_IDLE,
    BOOT_UPDATE_RECEIVING,
    BOOT_UPDATE_STAGED,
} boot_update_state_t;

static boot_update_state_t _boot_update_state = BOOT_UPDATE_IDLE;
static boot_stage_header_s _boot_update_header;
static uint32_t _boot_update_base;     // Where the stream starts in flash
static uint32_t _boot_update_total;    // The stream's bytes, whole sectors
static volatile uint32_t _boot_update_received;
static volatile uint32_t _boot_update_written;
static bool _boot_update_header_erased;
static uint64_t _boot_update_restart_at;

// Two sectors: one filling while the other waits for the task (the window is two sectors)
static uint8_t __attribute__((aligned(4))) _boot_update_buf[2][FLASH_SECTOR_SIZE];

static uint32_t _boot_update_align(uint32_t len)
{
    return ((len + FLASH_SECTOR_SIZE - 1u) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
}

static uint32_t _boot_update_crc32(const uint8_t *data, uint32_t len)
{
    // A nibble table, as the bootloader's
    static const uint32_t table[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
    }
    return ~crc;
}

static uint32_t _boot_update_running_end(void)
{
    return _boot_update_align((uint32_t)&__flash_binary_end - XIP_BASE);
}

// --- Flash, from the task (the other core waits out each write) ---

typedef struct
{
    uint32_t offset;
    const uint8_t *data;
    uint32_t len;  // 0: erase only
} boot_update_write_s;

static void _boot_update_flash(void *param)
{
    const boot_update_write_s *w = (const boot_update_write_s *)param;
    flash_range_erase(w->offset, FLASH_SECTOR_SIZE);
    if (w->len)
        flash_range_program(w->offset, w->data, w->len);
}

static bool _boot_update_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    boot_update_write_s w = {.offset = offset, .data = data, .len = len};
    return flash_safe_execute(_boot_update_flash, &w, UINT32_MAX) == PICO_OK;
}

// --- Public ---

void boot_update_info(boot_update_info_s *info)
{
    memset(info, 0, sizeof(*info));
    info->supported = true;
    info->app_offset = BOOT_LAYOUT_APP_OFFSET;
    info->radio_offset = BOOT_LAYOUT_RADIO_OFFSET;
    info->radio_size = BOOT_LAYOUT_RADIO_SIZE;
    info->stage_header_offset = BOOT_LAYOUT_STAGE_HEADER_OFFSET;
    info->running_end = _boot_update_running_end();
    if (BOOT_LAYOUT_RADIO_SIZE)
        info->radio_header_crc32 = _boot_update_crc32((const uint8_t *)(XIP_BASE + BOOT_LAYOUT_RADIO_OFFSET),
                                                      sizeof(boot_radio_header_s));

    battery_status_s battery = {0};
    fuelgauge_status_s gauge = {0};
    battery_get_status(&battery);
    fuelgauge_get_status(&gauge);
    info->battery_percent = gauge.connected ? gauge.percent : BOOT_UPDATE_NO_GAUGE;
    info->battery_ok = !battery.connected || battery.plugged || !gauge.connected ||
                       (gauge.percent >= BOOT_UPDATE_BATTERY_MIN_PERCENT);
}

boot_update_status_t boot_update_begin(const boot_update_part_s *parts, uint8_t count)
{
    boot_update_info_s info;
    boot_update_info(&info);

    if (!info.battery_ok)
        return BOOT_UPDATE_BATTERY_LOW;

    if ((count == 0) || (count > BOOT_UPDATE_PARTS_MAX))
        return BOOT_UPDATE_BAD_REQUEST;

    // The stream: each part's data in whole sectors, back to back, ending at the header
    uint32_t total = 0;
    uint32_t written_end = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        const boot_update_part_s *p = &parts[i];
        uint32_t room = (p->destination == BOOT_STAGE_TO_RADIO) ? BOOT_LAYOUT_RADIO_SIZE : BOOT_LAYOUT_APP_MAX;
        uint32_t dest = (p->destination == BOOT_STAGE_TO_RADIO) ? BOOT_LAYOUT_RADIO_OFFSET : BOOT_LAYOUT_APP_OFFSET;
        bool stored = (p->encoding == BOOT_STAGE_STORED);

        if ((p->destination > BOOT_STAGE_TO_RADIO) || (p->encoding > BOOT_STAGE_DEFLATE) ||
            (p->image_size == 0) || (p->image_size > room) || (p->staged_size == 0) ||
            (stored && (p->staged_size != p->image_size)))
            return BOOT_UPDATE_BAD_REQUEST;

        total += _boot_update_align(p->staged_size);
        if (dest + p->image_size > written_end)
            written_end = dest + p->image_size;
    }

    // Above this firmware (it runs on while the data arrives) and above everything the update writes
    if (total > BOOT_LAYOUT_STAGE_HEADER_OFFSET)
        return BOOT_UPDATE_NO_ROOM;
    uint32_t base = BOOT_LAYOUT_STAGE_HEADER_OFFSET - total;
    if ((base < info.running_end) || (base < written_end))
        return BOOT_UPDATE_NO_ROOM;

    boot_stage_header_s header = {
        .magic = BOOT_STAGE_MAGIC,
        .magic_inv = ~BOOT_STAGE_MAGIC,
        .version = BOOT_STAGE_VERSION,
        .part_count = count,
    };

    uint32_t at = base;
    for (uint8_t i = 0; i < count; i++)
    {
        boot_stage_part_s *s = &header.parts[i];
        s->destination = parts[i].destination;
        s->encoding = parts[i].encoding;
        s->staged_offset = at;
        s->staged_size = parts[i].staged_size;
        s->staged_crc32 = parts[i].staged_crc32;
        s->image_size = parts[i].image_size;
        s->image_crc32 = parts[i].image_crc32;
        at += _boot_update_align(parts[i].staged_size);
    }

    // The same update again (its link dropped, and the sender came back): it carries on from what is
    // written, where the answer says
    if ((_boot_update_state == BOOT_UPDATE_RECEIVING) && !memcmp(&header, &_boot_update_header, sizeof(header)))
    {
        _boot_update_received = _boot_update_written;
        return BOOT_UPDATE_OK;
    }

    _boot_update_header = header;
    _boot_update_base = base;
    _boot_update_total = total;
    _boot_update_received = 0;
    _boot_update_written = 0;
    // An older staged update must not apply to half of this one: the task erases its header first
    _boot_update_header_erased = false;
    _boot_update_state = BOOT_UPDATE_RECEIVING;
    return BOOT_UPDATE_OK;
}

boot_update_status_t boot_update_data(uint32_t offset, const uint8_t *data, uint32_t len)
{
    if (_boot_update_state != BOOT_UPDATE_RECEIVING)
        return BOOT_UPDATE_FAILED;

    // A resend from an earlier point (the sender missed an acknowledgement): what is here already is
    // skipped, so the stream carries on wherever the sender picks it up
    uint32_t received = _boot_update_received;
    if (offset > received)
        return BOOT_UPDATE_OUT_OF_ORDER;
    uint32_t have = received - offset;
    if (have >= len)
        return BOOT_UPDATE_OK;
    data += have;
    len -= have;

    if ((len > _boot_update_total - received) || (received + len - _boot_update_written > BOOT_UPDATE_WINDOW))
        return BOOT_UPDATE_OUT_OF_ORDER;

    for (uint32_t i = 0; i < len;)
    {
        uint32_t at = received + i;
        uint32_t in_sector = at % FLASH_SECTOR_SIZE;
        uint32_t n = FLASH_SECTOR_SIZE - in_sector;
        if (n > len - i)
            n = len - i;
        memcpy(&_boot_update_buf[(at / FLASH_SECTOR_SIZE) & 1u][in_sector], &data[i], n);
        i += n;
    }
    _boot_update_received = received + len;
    return BOOT_UPDATE_OK;
}

bool boot_update_task(uint64_t now_us)
{
    if (_boot_update_restart_at && (now_us >= _boot_update_restart_at))
    {
        // The next boot opens the beacon (the bootloader leaves this request for the firmware)
        watchdog_hw->scratch[BOOT_REQUEST_SCRATCH_IDX] = BOOT_REQUEST_BEACON;
        watchdog_hw->scratch[BOOT_REQUEST_SCRATCH_IDX + 1] = ~BOOT_REQUEST_BEACON;
        sys_hal_reboot();
    }

    if (_boot_update_state != BOOT_UPDATE_RECEIVING)
        return false;

    if (!_boot_update_header_erased)
    {
        _boot_update_header_erased = _boot_update_write(BOOT_LAYOUT_STAGE_HEADER_OFFSET, NULL, 0);
        return false;
    }

    // One sector a pass: each write holds the radio up for a few tens of ms
    uint32_t written = _boot_update_written;
    if (_boot_update_received - written < FLASH_SECTOR_SIZE)
        return false;

    const uint8_t *sector = _boot_update_buf[(written / FLASH_SECTOR_SIZE) & 1u];
    if (!_boot_update_write(_boot_update_base + written, sector, FLASH_SECTOR_SIZE))
        return false;
    _boot_update_written = written + FLASH_SECTOR_SIZE;
    return true;
}

boot_update_status_t boot_update_finish(void)
{
    if ((_boot_update_state != BOOT_UPDATE_RECEIVING) || (_boot_update_written != _boot_update_total))
        return BOOT_UPDATE_FAILED;

    for (uint8_t i = 0; i < _boot_update_header.part_count; i++)
    {
        const boot_stage_part_s *s = &_boot_update_header.parts[i];
        if (_boot_update_crc32((const uint8_t *)(XIP_BASE + s->staged_offset), s->staged_size) != s->staged_crc32)
        {
            _boot_update_state = BOOT_UPDATE_IDLE;
            return BOOT_UPDATE_FAILED;
        }
    }

    // The header last: from here the next boot writes the update
    static uint8_t __attribute__((aligned(4))) page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &_boot_update_header, sizeof(_boot_update_header));
    if (!_boot_update_write(BOOT_LAYOUT_STAGE_HEADER_OFFSET, page, sizeof(page)))
    {
        _boot_update_state = BOOT_UPDATE_IDLE;
        return BOOT_UPDATE_FAILED;
    }

    _boot_update_state = BOOT_UPDATE_STAGED;
    _boot_update_restart_at = sys_hal_now_us() + BOOT_UPDATE_RESTART_US;
    return BOOT_UPDATE_RESTARTING;
}

void boot_update_abort(void)
{
    if (_boot_update_state == BOOT_UPDATE_RECEIVING)
        _boot_update_state = BOOT_UPDATE_IDLE;
}

uint32_t boot_update_written(void)
{
    return _boot_update_written;
}

bool boot_update_active(void)
{
    return _boot_update_state != BOOT_UPDATE_IDLE;
}

bool boot_update_take_beacon(void)
{
    io_rw_32 *scratch = &watchdog_hw->scratch[BOOT_REQUEST_SCRATCH_IDX];
    bool requested = (scratch[0] == BOOT_REQUEST_BEACON) && (scratch[1] == ~BOOT_REQUEST_BEACON);
    if (requested)
    {
        scratch[0] = 0;
        scratch[1] = 0;
    }
    return requested;
}

#else

void boot_update_info(boot_update_info_s *info)
{
    memset(info, 0, sizeof(*info));
}

boot_update_status_t boot_update_begin(const boot_update_part_s *parts, uint8_t count)
{
    (void)parts;
    (void)count;
    return BOOT_UPDATE_UNSUPPORTED;
}

boot_update_status_t boot_update_data(uint32_t offset, const uint8_t *data, uint32_t len)
{
    (void)offset;
    (void)data;
    (void)len;
    return BOOT_UPDATE_UNSUPPORTED;
}

boot_update_status_t boot_update_finish(void)
{
    return BOOT_UPDATE_UNSUPPORTED;
}

void boot_update_abort(void)
{
}

bool boot_update_task(uint64_t now_us)
{
    (void)now_us;
    return false;
}

uint32_t boot_update_written(void)
{
    return 0;
}

bool boot_update_active(void)
{
    return false;
}

bool boot_update_take_beacon(void)
{
    return false;
}

#endif
