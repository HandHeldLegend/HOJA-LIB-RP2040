#include "hal/record_hal.h"

#include <stddef.h>
#include <string.h>

#include "flash_layout.h"
#include "hal/flash_hal.h"
#include "nrf52_hal.h"
#include "utilities/crc32.h"

// The record store of rp2040/record_hal.c, in the board's storage area (flash_layout.h): the same
// records, whole sectors with a sequence number, the newest whole copy of each id winning.

#define FLASH_SECTOR_SIZE           NRF52_FLASH_SECTOR_SIZE
#define RECORD_HAL_POOL_OFFSET      NRF52_FLASH_RECORDS_OFFSET
#define RECORD_HAL_POOL_SECTORS     NRF52_FLASH_RECORDS_SECTORS
#define RECORD_HAL_MAGIC            0x43455248u // "HREC"
#define RECORD_HAL_NONE             0xFFu       // No sector
#define RECORD_HAL_LOCKOUT_MS       0u          // One core: nothing to wait for

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

// Over an update. A board's updater may erase the store along with the firmware (Valve's bootloader
// on the Steam Controller erases all the flash it can write). The newest copies then wait in RAM the
// updater leaves alone and the firmware's own memory stays clear of, named by the board's linker
// script: __record_carry, of __record_carry_size bytes (a symbol whose address is the size). Without
// the symbol nothing is carried.
extern uint8_t __record_carry[] __attribute__((weak));
extern uint8_t __record_carry_size[] __attribute__((weak));

static uint8_t _record_hal_current[RECORD_HAL_IDS]; // The sector holding each id's newest copy
static bool _record_hal_scanned = false;
static uint32_t _record_hal_seq;                     // The newest sequence number seen
static uint8_t _record_hal_next;                     // Where the next write looks first (wear spreads)
static uint8_t __attribute__((aligned(4))) _record_hal_sector[FLASH_SECTOR_SIZE];

static const record_hal_header_s *_record_hal_at(uint8_t sector)
{
    return (const record_hal_header_s *)(RECORD_HAL_POOL_OFFSET + sector * FLASH_SECTOR_SIZE);
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

static void _record_hal_scan(void)
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
    _record_hal_scanned = true;
}

// A carried record's length: its header and payload, to a whole word
static uint32_t _record_hal_carried_len(const record_hal_header_s *h)
{
    return (uint32_t)sizeof(*h) + ((h->length + 3u) & ~3u);
}

// What an update carried goes back into a store it left empty. Once: the copies in RAM are spent
// here either way, so a store someone empties later stays empty.
static void _record_hal_carry_back(void)
{
    if (!__record_carry)
        return;

    bool empty = true;
    for (uint16_t id = 0; id < RECORD_HAL_IDS; id++)
        empty &= (_record_hal_current[id] == RECORD_HAL_NONE);

    const uint8_t *end = __record_carry + (uint32_t)(uintptr_t)__record_carry_size;
    const uint8_t *at = __record_carry;
    while (empty && (at + sizeof(record_hal_header_s) <= end))
    {
        const record_hal_header_s *h = (const record_hal_header_s *)at;
        if ((h->magic != RECORD_HAL_MAGIC) || (h->length > RECORD_HAL_PAYLOAD_MAX_LEN) ||
            (at + _record_hal_carried_len(h) > end) || !_record_hal_is_whole(h))
            break;

        if (_record_hal_current[h->id] == RECORD_HAL_NONE)
        {
            const record_hal_part_s part = {.data = h + 1, .len = h->length};
            record_hal_write(h->id, &part, 1);
        }
        at += _record_hal_carried_len(h);
    }
    memset(__record_carry, 0, sizeof(record_hal_header_s));
}

void nrf52_record_carry(void)
{
    if (!__record_carry)
        return;
    // Asked before the store was read (a button combination at start)
    if (!_record_hal_scanned)
        _record_hal_scan();

    // Back to back, lowest id first, ended by a header that is none. One that no longer fits is
    // left behind.
    uint8_t *end = __record_carry + (uint32_t)(uintptr_t)__record_carry_size;
    uint8_t *at = __record_carry;
    for (uint16_t id = 0; id < RECORD_HAL_IDS; id++)
    {
        if (_record_hal_current[id] == RECORD_HAL_NONE)
            continue;
        const record_hal_header_s *h = _record_hal_at(_record_hal_current[id]);
        const uint32_t len = _record_hal_carried_len(h);
        if (at + len + sizeof(record_hal_header_s) > end)
            continue;
        memcpy(at, h, len);
        at += len;
    }
    memset(at, 0, sizeof(record_hal_header_s));
}

bool record_hal_init(void)
{
    _record_hal_scan();
    _record_hal_carry_back();
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
