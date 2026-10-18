#include "hal/record_hal.h"

#include <stddef.h>

#if defined(HOJA_BOOTLOADER)

#include <string.h>

#include "hardware/flash.h"

#include "board_config.h"
#include "hal/flash_hal.h"
#include "hal/rp2040/boot_layout.h"
#include "hal/rp2040/bt_flash_bank.h"
#include "utilities/crc32.h"

// flash_hal keeps its pages below the Bluetooth pairings only with FLOSS
#if !(defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_FLOSS)
#error "record_hal: a bootloader board without FLOSS needs its own end for the pool"
#endif

// The pool: the bootloader's top area, short of flash_hal's two pages below the Bluetooth pairings:
// page 0, the settings from before profiles (left as they were, so older firmware still finds them),
// and page 1, kept for Mii data
#define RECORD_HAL_FLASH_HAL_PAGES 2u
#define RECORD_HAL_POOL_OFFSET     BOOT_LAYOUT_TOP_OFFSET
#define RECORD_HAL_POOL_END        (BT_FLASH_BANK_OFFSET - RECORD_HAL_FLASH_HAL_PAGES * FLASH_SECTOR_SIZE)
#define RECORD_HAL_POOL_SECTORS    ((RECORD_HAL_POOL_END - RECORD_HAL_POOL_OFFSET) / FLASH_SECTOR_SIZE)
#define RECORD_HAL_MAGIC           0x43455248u // "HREC"
#define RECORD_HAL_NONE            0xFFu       // No sector
// The other core's time to stop for a write; a busy core fails the write, which is tried again
#define RECORD_HAL_LOCKOUT_MS      100u

typedef struct
{
    uint32_t magic;
    uint16_t id;
    uint16_t reserved;
    uint32_t seq;
    uint32_t length;
    uint32_t crc32; // Over the payload
} record_hal_header_s;

_Static_assert(sizeof(record_hal_header_s) == RECORD_HAL_HEADER_LEN, "record header size");
_Static_assert(RECORD_HAL_SECTOR_LEN == FLASH_SECTOR_SIZE, "a record is one flash sector");
_Static_assert(RECORD_HAL_IDS < RECORD_HAL_POOL_SECTORS, "a write needs a sector no newest copy holds");
_Static_assert(RECORD_HAL_POOL_SECTORS < RECORD_HAL_NONE, "a sector index fits a byte");

static uint8_t _record_hal_current[RECORD_HAL_IDS]; // The sector holding each id's newest copy
static uint32_t _record_hal_seq;                     // The newest sequence number seen
static uint8_t _record_hal_next;                     // Where the next write looks first (wear spreads)
static uint8_t __attribute__((aligned(4))) _record_hal_sector[FLASH_SECTOR_SIZE];

static const record_hal_header_s *_record_hal_at(uint8_t sector)
{
    return (const record_hal_header_s *)(XIP_BASE + RECORD_HAL_POOL_OFFSET + sector * FLASH_SECTOR_SIZE);
}

static bool _record_hal_is_whole(const record_hal_header_s *h)
{
    return (h->magic == RECORD_HAL_MAGIC) && (h->id < RECORD_HAL_IDS) &&
           (h->length <= RECORD_HAL_PAYLOAD_MAX_LEN) &&
           (crc32_compute((const uint8_t *)(h + 1), h->length) == h->crc32);
}

// A sector no id's newest copy sits in, from where the last write left off
static uint8_t _record_hal_free_sector(void)
{
    for (uint8_t i = 0; i < RECORD_HAL_POOL_SECTORS; i++)
    {
        uint8_t s = (uint8_t)((_record_hal_next + i) % RECORD_HAL_POOL_SECTORS);
        if (!memchr(_record_hal_current, s, sizeof(_record_hal_current)))
            return s;
    }
    return RECORD_HAL_NONE;
}

bool record_hal_init(void)
{
    memset(_record_hal_current, RECORD_HAL_NONE, sizeof(_record_hal_current));
    _record_hal_seq = 0;

    for (uint8_t s = 0; s < RECORD_HAL_POOL_SECTORS; s++)
    {
        const record_hal_header_s *h = _record_hal_at(s);
        if (!_record_hal_is_whole(h))
            continue;

        uint8_t held = _record_hal_current[h->id];
        if ((held == RECORD_HAL_NONE) || (h->seq > _record_hal_at(held)->seq))
            _record_hal_current[h->id] = s;
        if (h->seq > _record_hal_seq)
        {
            _record_hal_seq = h->seq;
            _record_hal_next = (uint8_t)((s + 1u) % RECORD_HAL_POOL_SECTORS);
        }
    }
    return true;
}

const uint8_t *record_hal_find(uint16_t id, uint32_t *len)
{
    if ((id >= RECORD_HAL_IDS) || (_record_hal_current[id] == RECORD_HAL_NONE))
        return NULL;

    const record_hal_header_s *h = _record_hal_at(_record_hal_current[id]);
    if (len)
        *len = h->length;
    return (const uint8_t *)(h + 1);
}

bool record_hal_write(uint16_t id, const record_hal_part_s *parts, uint8_t count)
{
    uint32_t len = 0;
    for (uint8_t i = 0; i < count; i++)
        len += parts[i].len;
    if ((id >= RECORD_HAL_IDS) || (count > RECORD_HAL_PARTS_MAX) || (len > RECORD_HAL_PAYLOAD_MAX_LEN))
        return false;

    uint8_t sector = _record_hal_free_sector();
    if (sector == RECORD_HAL_NONE)
        return false;
    // The next write looks elsewhere first, whether this one reads back whole or not
    _record_hal_next = (uint8_t)((sector + 1u) % RECORD_HAL_POOL_SECTORS);

    memset(_record_hal_sector, 0xFF, sizeof(_record_hal_sector));
    uint8_t *payload = &_record_hal_sector[RECORD_HAL_HEADER_LEN];
    uint32_t at = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        memcpy(&payload[at], parts[i].data, parts[i].len);
        at += parts[i].len;
    }

    record_hal_header_s header = {
        .magic = RECORD_HAL_MAGIC,
        .id = id,
        .reserved = 0xFFFFu,
        .seq = _record_hal_seq + 1u,
        .length = len,
        .crc32 = crc32_compute(payload, len),
    };
    memcpy(_record_hal_sector, &header, sizeof(header));

    const uint32_t offset = RECORD_HAL_POOL_OFFSET + sector * FLASH_SECTOR_SIZE;
    if (!flash_hal_write_sector(offset, _record_hal_sector, FLASH_SECTOR_SIZE, RECORD_HAL_LOCKOUT_MS))
        return false;

    // Only a copy that reads back whole replaces the one before
    if (!_record_hal_is_whole(_record_hal_at(sector)))
        return false;

    _record_hal_seq = header.seq;
    _record_hal_current[id] = sector;
    return true;
}

#else

// No store on this board

bool record_hal_init(void)
{
    return false;
}

const uint8_t *record_hal_find(uint16_t id, uint32_t *len)
{
    (void)id;
    (void)len;
    return NULL;
}

bool record_hal_write(uint16_t id, const record_hal_part_s *parts, uint8_t count)
{
    (void)id;
    (void)parts;
    (void)count;
    return false;
}

#endif
