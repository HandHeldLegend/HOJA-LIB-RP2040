#include "hal/rp2040/boot_update.h"

#include <string.h>

#if defined(HOJA_BOOTLOADER)

#include "hardware/flash.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "hal/flash_hal.h"
#include "hal/rp2040/boot_layout.h"
#include "hal/sys_hal.h"
#include "utilities/crc32.h"

// The reply leaves before the restart
#define BOOT_UPDATE_RESTART_US          300000u
// The battery an update needs, unless the controller is plugged in: the first boot after an update
// writes it into place, and a battery that runs out part way leaves the controller unable to start
// until it is charged (the next boot then writes the update again)
#define BOOT_UPDATE_BATTERY_MIN_PERCENT 30u
// A write waits this long for the other core to stop; a write that didn't happen is tried again
#define BOOT_UPDATE_FLASH_WAIT_MS       100u

_Static_assert(BOOT_UPDATE_PARTS_MAX <= BOOT_STAGE_PARTS_MAX, "more parts than the stage header holds");

// The end of this firmware in flash (the linker's)
extern char __flash_binary_end;

typedef enum
{
    BOOT_UPDATE_STATE_IDLE,
    BOOT_UPDATE_STATE_RECEIVING,
    BOOT_UPDATE_STATE_STAGED,    // The header is written: the restart is due
} boot_update_state_t;

static boot_update_state_t _boot_update_state = BOOT_UPDATE_STATE_IDLE;
static boot_stage_header_s _boot_update_header;
static uint32_t _boot_update_base;     // Where the stream starts in flash
static uint32_t _boot_update_total;    // The stream's bytes, whole sectors
static volatile uint32_t _boot_update_received;
static volatile uint32_t _boot_update_written;
static bool _boot_update_header_erased;
static uint64_t _boot_update_restart_at_us;

// The stream's next sectors, one filling while the one before it waits for the task
static uint8_t __attribute__((aligned(4))) _boot_update_buf[BOOT_UPDATE_BUFFER_COUNT][FLASH_SECTOR_SIZE];

static uint8_t *_boot_update_sector_buf(uint32_t stream_offset)
{
    return _boot_update_buf[(stream_offset / FLASH_SECTOR_SIZE) % BOOT_UPDATE_BUFFER_COUNT];
}

static uint32_t _boot_update_running_end(void)
{
    return boot_layout_sector_align((uint32_t)&__flash_binary_end - XIP_BASE);
}

// One sector, from the task (the other core waits out the write). False when the other core didn't
// stop in time: nothing was written.
static bool _boot_update_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    return flash_hal_write_sector(offset, data, len, BOOT_UPDATE_FLASH_WAIT_MS);
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
        info->radio_header_crc32 = crc32_compute((const uint8_t *)(XIP_BASE + BOOT_LAYOUT_RADIO_OFFSET),
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
    // A new stream would write under the staged header, moments before the restart
    if (_boot_update_state == BOOT_UPDATE_STATE_STAGED)
        return BOOT_UPDATE_RESTARTING;

    boot_update_info_s info;
    boot_update_info(&info);

    if (!info.battery_ok)
        return BOOT_UPDATE_BATTERY_LOW;

    if ((count == 0) || (count > BOOT_UPDATE_PARTS_MAX))
        return BOOT_UPDATE_BAD_REQUEST;

    // The stream: each part's data in whole sectors, back to back, ending at the header. A part
    // larger than the whole stage area is refused before its size is rounded up (it could wrap).
    uint32_t total = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        if (parts[i].staged_size > BOOT_LAYOUT_APP_MAX)
            return BOOT_UPDATE_NO_ROOM;

        total += boot_layout_sector_align(parts[i].staged_size);
        if (total > BOOT_LAYOUT_APP_MAX)
            return BOOT_UPDATE_NO_ROOM;
    }
    uint32_t base = BOOT_LAYOUT_STAGE_HEADER_OFFSET - total;

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
        at += boot_layout_sector_align(parts[i].staged_size);
    }

    // As the bootloader will check it, which also keeps the stream above everything it writes
    switch (boot_stage_header_check(&header))
    {
        case BOOT_STAGE_VALID:
            break;

        case BOOT_STAGE_NO_ROOM:
            return BOOT_UPDATE_NO_ROOM;

        default:
            return BOOT_UPDATE_BAD_REQUEST;
    }

    // And above this firmware, which runs on while the data arrives
    if (base < info.running_end)
        return BOOT_UPDATE_NO_ROOM;

    // The same update again (its link dropped, and the sender came back): it carries on from what is
    // written, where the answer says
    if ((_boot_update_state == BOOT_UPDATE_STATE_RECEIVING) &&
        !memcmp(&header, &_boot_update_header, sizeof(header)))
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
    _boot_update_state = BOOT_UPDATE_STATE_RECEIVING;
    return BOOT_UPDATE_OK;
}

boot_update_status_t boot_update_data(uint32_t offset, const uint8_t *data, uint32_t len)
{
    if (_boot_update_state != BOOT_UPDATE_STATE_RECEIVING)
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
        memcpy(&_boot_update_sector_buf(at)[in_sector], &data[i], n);
        i += n;
    }
    _boot_update_received = received + len;
    return BOOT_UPDATE_OK;
}

bool boot_update_task(uint64_t now_us)
{
    if ((_boot_update_state == BOOT_UPDATE_STATE_STAGED) && (now_us >= _boot_update_restart_at_us))
    {
        // The next boot opens the beacon (the bootloader leaves this request for the firmware)
        boot_request_set(BOOT_REQUEST_BEACON);
        sys_hal_reboot();
    }

    if (_boot_update_state != BOOT_UPDATE_STATE_RECEIVING)
        return false;

    if (!_boot_update_header_erased)
    {
        _boot_update_header_erased = _boot_update_write(BOOT_LAYOUT_STAGE_HEADER_OFFSET, NULL, 0);
        return false;
    }

    // One sector a pass: each write stops the other core and holds up the radio
    uint32_t written = _boot_update_written;
    if (_boot_update_received - written < FLASH_SECTOR_SIZE)
        return false;

    if (!_boot_update_write(_boot_update_base + written, _boot_update_sector_buf(written), FLASH_SECTOR_SIZE))
        return false;
    _boot_update_written = written + FLASH_SECTOR_SIZE;
    return true;
}

boot_update_status_t boot_update_finish(void)
{
    if (_boot_update_state == BOOT_UPDATE_STATE_STAGED)
        return BOOT_UPDATE_RESTARTING;

    if (_boot_update_state != BOOT_UPDATE_STATE_RECEIVING)
        return BOOT_UPDATE_FAILED;

    // Data still to come, or not yet written: the sender resends from what is written
    if (_boot_update_written != _boot_update_total)
        return BOOT_UPDATE_OUT_OF_ORDER;

    for (uint8_t i = 0; i < _boot_update_header.part_count; i++)
    {
        const boot_stage_part_s *s = &_boot_update_header.parts[i];
        if (crc32_compute((const uint8_t *)(XIP_BASE + s->staged_offset), s->staged_size) != s->staged_crc32)
        {
            _boot_update_state = BOOT_UPDATE_STATE_IDLE;
            return BOOT_UPDATE_FAILED;
        }
    }

    // The header last: from here the next boot writes the update. Not written, the update stays
    // received: begun again, it carries on at its end, ready to finish.
    static uint8_t __attribute__((aligned(4))) page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &_boot_update_header, sizeof(_boot_update_header));
    if (!_boot_update_write(BOOT_LAYOUT_STAGE_HEADER_OFFSET, page, sizeof(page)))
        return BOOT_UPDATE_FAILED;

    _boot_update_state = BOOT_UPDATE_STATE_STAGED;
    _boot_update_restart_at_us = sys_hal_now_us() + BOOT_UPDATE_RESTART_US;
    return BOOT_UPDATE_RESTARTING;
}

uint32_t boot_update_written(void)
{
    return _boot_update_written;
}

bool boot_update_active(void)
{
    return _boot_update_state != BOOT_UPDATE_STATE_IDLE;
}

bool boot_update_take_beacon(void)
{
    return boot_request_take(BOOT_REQUEST_BEACON);
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
