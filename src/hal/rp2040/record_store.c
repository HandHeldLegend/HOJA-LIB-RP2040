#include "hal/rp2040/record_store.h"

#include <stddef.h>

#if defined(HOJA_BOOTLOADER)

#include <string.h>

#include "pico/flash.h"
#include "hardware/flash.h"

#include "hal/rp2040/boot_layout.h"

// The pool: the top area up to the two sectors below the Bluetooth pairings (the old settings page,
// left as it was so older firmware still finds its settings, and the Mii page)
#define RECORD_POOL_OFFSET  BOOT_LAYOUT_TOP_OFFSET
#define RECORD_POOL_END     (PICO_FLASH_SIZE_BYTES - 4u * FLASH_SECTOR_SIZE)
#define RECORD_POOL_SECTORS ((RECORD_POOL_END - RECORD_POOL_OFFSET) / FLASH_SECTOR_SIZE)
#define RECORD_MAGIC        0x43455248u // "HREC"
#define RECORD_NONE         0xFFu

typedef struct
{
    uint32_t magic;
    uint16_t id;
    uint16_t reserved;
    uint32_t seq;
    uint32_t length;
    uint32_t crc32; // Over the payload
} record_header_s;

_Static_assert(sizeof(record_header_s) == RECORD_STORE_HEADER_LEN, "record header size");

static uint8_t _record_current[RECORD_STORE_IDS]; // The sector holding each id's newest copy
static uint32_t _record_seq;                       // The newest sequence number seen
static uint8_t _record_next;                       // Where the next write looks first (wear spreads round)
static uint8_t __attribute__((aligned(4))) _record_sector[FLASH_SECTOR_SIZE];

static uint32_t _record_crc32(const uint8_t *data, uint32_t len)
{
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

static const record_header_s *_record_at(uint8_t sector)
{
    return (const record_header_s *)(XIP_BASE + RECORD_POOL_OFFSET + sector * FLASH_SECTOR_SIZE);
}

static bool _record_whole(const record_header_s *h)
{
    return (h->magic == RECORD_MAGIC) && (h->id < RECORD_STORE_IDS) && (h->length <= RECORD_STORE_PAYLOAD_MAX) &&
           (_record_crc32((const uint8_t *)(h + 1), h->length) == h->crc32);
}

void record_store_init(void)
{
    memset(_record_current, RECORD_NONE, sizeof(_record_current));
    _record_seq = 0;

    for (uint8_t s = 0; s < RECORD_POOL_SECTORS; s++)
    {
        const record_header_s *h = _record_at(s);
        if (!_record_whole(h))
            continue;

        uint8_t held = _record_current[h->id];
        if ((held == RECORD_NONE) || (h->seq > _record_at(held)->seq))
            _record_current[h->id] = s;
        if (h->seq > _record_seq)
        {
            _record_seq = h->seq;
            _record_next = (uint8_t)((s + 1u) % RECORD_POOL_SECTORS);
        }
    }
}

const uint8_t *record_store_find(uint16_t id, uint32_t *len)
{
    if ((id >= RECORD_STORE_IDS) || (_record_current[id] == RECORD_NONE))
        return NULL;

    const record_header_s *h = _record_at(_record_current[id]);
    if (len)
        *len = h->length;
    return (const uint8_t *)(h + 1);
}

static void _record_flash(void *param)
{
    uint32_t offset = *(const uint32_t *)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    flash_range_program(offset, _record_sector, FLASH_SECTOR_SIZE);
}

bool record_store_write(uint16_t id, const void *a, uint32_t a_len, const void *b, uint32_t b_len)
{
    uint32_t len = a_len + (b ? b_len : 0);
    if ((id >= RECORD_STORE_IDS) || (len > RECORD_STORE_PAYLOAD_MAX))
        return false;

    // A sector no id's newest copy sits in, from where the last write left off
    uint8_t sector = RECORD_NONE;
    for (uint8_t i = 0; i < RECORD_POOL_SECTORS; i++)
    {
        uint8_t s = (uint8_t)((_record_next + i) % RECORD_POOL_SECTORS);
        if (!memchr(_record_current, s, sizeof(_record_current)))
        {
            sector = s;
            break;
        }
    }
    if (sector == RECORD_NONE)
        return false;

    memset(_record_sector, 0xFF, sizeof(_record_sector));
    uint8_t *payload = &_record_sector[RECORD_STORE_HEADER_LEN];
    memcpy(payload, a, a_len);
    if (b)
        memcpy(&payload[a_len], b, b_len);

    record_header_s header = {
        .magic = RECORD_MAGIC,
        .id = id,
        .reserved = 0xFFFFu,
        .seq = _record_seq + 1u,
        .length = len,
        .crc32 = _record_crc32(payload, len),
    };
    memcpy(_record_sector, &header, sizeof(header));

    uint32_t offset = RECORD_POOL_OFFSET + sector * FLASH_SECTOR_SIZE;
    if (flash_safe_execute(_record_flash, &offset, UINT32_MAX) != PICO_OK)
        return false;

    // Only a copy that reads back whole replaces the one before
    if (!_record_whole(_record_at(sector)))
        return false;

    _record_seq = header.seq;
    _record_current[id] = sector;
    _record_next = (uint8_t)((sector + 1u) % RECORD_POOL_SECTORS);
    return true;
}

#else

void record_store_init(void)
{
}

const uint8_t *record_store_find(uint16_t id, uint32_t *len)
{
    (void)id;
    (void)len;
    return NULL;
}

bool record_store_write(uint16_t id, const void *a, uint32_t a_len, const void *b, uint32_t b_len)
{
    (void)id;
    (void)a;
    (void)a_len;
    (void)b;
    (void)b_len;
    return false;
}

#endif
