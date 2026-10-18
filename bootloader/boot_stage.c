#include "boot_stage.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hardware/flash.h"

#include "hal/rp2040/boot_layout.h"
#include "utilities/crc32.h"
#include "boot_flash.h"
#include "boot_inflate.h"
#include "boot_rgb.h"

// --- Output: a sector at a time into the destination ---

typedef struct
{
    uint32_t offset;   // Next sector to write
    uint32_t fill;     // Bytes in the sector buffer
    uint32_t written;  // Bytes put so far
    uint32_t limit;    // The header's image size
} boot_stage_out_s;

static uint8_t __attribute__((aligned(4))) _boot_stage_sector[FLASH_SECTOR_SIZE];
static boot_stage_out_s _boot_stage_out;

static void _boot_stage_out_flush(void)
{
    if (_boot_stage_out.fill == 0)
        return;

    memset(&_boot_stage_sector[_boot_stage_out.fill], 0xFF, FLASH_SECTOR_SIZE - _boot_stage_out.fill);
    boot_flash_write_sector(_boot_stage_out.offset, _boot_stage_sector);
    _boot_stage_out.offset += FLASH_SECTOR_SIZE;
    _boot_stage_out.fill = 0;
    boot_rgb_task(BOOT_RGB_WRITING);
}

static bool _boot_stage_out_put(uint8_t value)
{
    if (_boot_stage_out.written >= _boot_stage_out.limit)
        return false;

    _boot_stage_sector[_boot_stage_out.fill++] = value;
    _boot_stage_out.written++;
    if (_boot_stage_out.fill == FLASH_SECTOR_SIZE)
        _boot_stage_out_flush();
    return true;
}

static bool _boot_stage_copy(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (!_boot_stage_out_put(data[i]))
            return false;
    }
    return true;
}

// --- Apply ---

static bool _boot_stage_data_whole(const boot_stage_header_s *header)
{
    for (uint8_t i = 0; i < header->part_count; i++)
    {
        const boot_stage_part_s *part = &header->parts[i];
        const uint8_t *data = (const uint8_t *)(XIP_BASE + part->staged_offset);
        if (crc32_compute(data, part->staged_size) != part->staged_crc32)
            return false;
        boot_rgb_task(BOOT_RGB_WRITING);
    }
    return true;
}

// Write one part into place. A result short of its size or unlike its CRC must not run: without
// its first sector the firmware is invalid (the update mode takes over), and the radio's header
// unmatched (the firmware leaves the radio off).
static void _boot_stage_write_part(const boot_stage_part_s *part)
{
    boot_stage_region_s region;
    if (!boot_stage_region(part->destination, &region))
        return;

    const uint8_t *data = (const uint8_t *)(XIP_BASE + part->staged_offset);
    const uint8_t *image = (const uint8_t *)(XIP_BASE + region.offset);

    memset(&_boot_stage_out, 0, sizeof(_boot_stage_out));
    _boot_stage_out.offset = region.offset;
    _boot_stage_out.limit = part->image_size;

    bool ok;
    if (part->encoding == BOOT_STAGE_DEFLATE)
        ok = boot_inflate(data, part->staged_size, _boot_stage_out_put);
    else
        ok = _boot_stage_copy(data, part->staged_size);
    _boot_stage_out_flush();

    ok = ok && (_boot_stage_out.written == part->image_size) &&
         (crc32_compute(image, part->image_size) == part->image_crc32);
    if (!ok)
        boot_flash_erase(region.offset, FLASH_SECTOR_SIZE);
}

void boot_stage_apply(void)
{
    const boot_stage_header_s *header =
        (const boot_stage_header_s *)(XIP_BASE + BOOT_LAYOUT_STAGE_HEADER_OFFSET);

    if (!boot_stage_header_whole(header))
        return;

    boot_rgb_task(BOOT_RGB_WRITING);

    // All parts, or none: a part that can't be written leaves everything as it was
    if ((boot_stage_header_check(header) == BOOT_STAGE_VALID) && _boot_stage_data_whole(header))
    {
        for (uint8_t i = 0; i < header->part_count; i++)
            _boot_stage_write_part(&header->parts[i]);
    }

    // Written, or not worth writing: either way it is done with
    boot_flash_erase(BOOT_LAYOUT_STAGE_HEADER_OFFSET, FLASH_SECTOR_SIZE);
}
