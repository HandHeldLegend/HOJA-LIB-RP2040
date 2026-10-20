#include "boot_uf2.h"

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"

#include "tusb.h"

#include "hal/rp2040/boot_layout.h"
#include "boot_usb.h"

#if !defined(BOOT_DEVICE_NAME) || !defined(BOOT_BUILD_ID)
#error "boot_uf2.c: hoja_use_bootloader sets BOOT_DEVICE_NAME and BOOT_BUILD_ID for each board"
#endif

// --- The volume ---

// FAT16, a sector to a cluster. Nothing is stored: every sector is made up when read, and writes
// are looked at only for UF2 blocks. 64000 sectors (31 MB) take a whole 16 MB flash's .uf2 file.
#define BOOT_UF2_SECTOR_LEN    512u
#define BOOT_UF2_SECTORS       64000u
#define BOOT_UF2_FAT_COUNT     2u
#define BOOT_UF2_FAT_SECTORS   250u
#define BOOT_UF2_ROOT_ENTRIES  64u
#define BOOT_UF2_DIRENT_LEN    32u
#define BOOT_UF2_ROOT_SECTORS  (BOOT_UF2_ROOT_ENTRIES * BOOT_UF2_DIRENT_LEN / BOOT_UF2_SECTOR_LEN)
#define BOOT_UF2_FAT_START     1u // After the boot sector
#define BOOT_UF2_ROOT_START    (BOOT_UF2_FAT_START + BOOT_UF2_FAT_COUNT * BOOT_UF2_FAT_SECTORS)
#define BOOT_UF2_DATA_START    (BOOT_UF2_ROOT_START + BOOT_UF2_ROOT_SECTORS)
#define BOOT_UF2_CLUSTERS      (BOOT_UF2_SECTORS - BOOT_UF2_DATA_START)
#define BOOT_UF2_LABEL         "HOJA UPDATE"
#define BOOT_UF2_SERIAL        0x41484F4Au

// FAT16 by its cluster count (anything else is FAT12 or FAT32 to the host), and a FAT that maps
// every cluster
_Static_assert((BOOT_UF2_CLUSTERS >= 4085u) && (BOOT_UF2_CLUSTERS < 65525u), "not a FAT16 volume");
_Static_assert((BOOT_UF2_CLUSTERS + 2u) * 2u <= BOOT_UF2_FAT_SECTORS * BOOT_UF2_SECTOR_LEN, "FAT too short");
_Static_assert(sizeof(BOOT_UF2_LABEL) - 1u == 11u, "a FAT label is 11 characters");

typedef struct __attribute__((packed))
{
    char     name[11]; // 8.3, space padded, no dot
    uint8_t  attr;
    uint8_t  reserved[8];
    uint16_t cluster_high;
    uint16_t time;
    uint16_t date;
    uint16_t cluster;
    uint32_t size;
} boot_uf2_dirent_s;

_Static_assert(sizeof(boot_uf2_dirent_s) == BOOT_UF2_DIRENT_LEN, "a FAT directory entry is 32 bytes");

#define BOOT_UF2_ATTR_READ_ONLY 0x01u
#define BOOT_UF2_ATTR_LABEL     0x08u

typedef struct
{
    char        name[12];
    const char *text;
    uint32_t    len;
} boot_uf2_file_s;

static const char _boot_uf2_info[] =
    "UF2 Bootloader HOJA\r\n"
    "Model: " BOOT_DEVICE_NAME "\r\n"
    "Board-ID: " BOOT_BUILD_ID "\r\n";

static const char _boot_uf2_index[] =
    "<!doctype html>\n"
    "<html><head><meta http-equiv=\"refresh\" content=\"0; url=https://handheldlegend.github.io/hoja3/\">"
    "</head><body><a href=\"https://handheldlegend.github.io/hoja3/\">HOJA config app</a></body></html>\n";

// One cluster each, from cluster 2 in order
static const boot_uf2_file_s _boot_uf2_files[] = {
    {"INFO_UF2TXT", _boot_uf2_info, sizeof(_boot_uf2_info) - 1u},
    {"INDEX   HTM", _boot_uf2_index, sizeof(_boot_uf2_index) - 1u},
};

#define BOOT_UF2_FILE_COUNT (sizeof(_boot_uf2_files) / sizeof(_boot_uf2_files[0]))

_Static_assert(sizeof(_boot_uf2_info) <= BOOT_UF2_SECTOR_LEN, "INFO_UF2.TXT outgrew its cluster");
_Static_assert(sizeof(_boot_uf2_index) <= BOOT_UF2_SECTOR_LEN, "INDEX.HTM outgrew its cluster");

// The boot sector up to its signature: jump, OEM name, then the BIOS parameter block
static const uint8_t _boot_uf2_boot_sector[] = {
    0xEB, 0x3C, 0x90,
    'H', 'O', 'J', 'A', ' ', ' ', ' ', ' ',
    U16_TO_U8S_LE(BOOT_UF2_SECTOR_LEN),
    1,                                    // Sectors a cluster
    U16_TO_U8S_LE(BOOT_UF2_FAT_START),    // Reserved sectors
    BOOT_UF2_FAT_COUNT,
    U16_TO_U8S_LE(BOOT_UF2_ROOT_ENTRIES),
    U16_TO_U8S_LE(BOOT_UF2_SECTORS),
    0xF8,                                 // Fixed disk
    U16_TO_U8S_LE(BOOT_UF2_FAT_SECTORS),
    U16_TO_U8S_LE(1),                     // Sectors a track
    U16_TO_U8S_LE(1),                     // Heads
    U32_TO_U8S_LE(0),                     // Hidden sectors
    U32_TO_U8S_LE(0),                     // Sectors, when over 65535
    0x80,                                 // Drive number
    0,
    0x29,                                 // The serial, label and type follow
    U32_TO_U8S_LE(BOOT_UF2_SERIAL),
    'H', 'O', 'J', 'A', ' ', 'U', 'P', 'D', 'A', 'T', 'E',
    'F', 'A', 'T', '1', '6', ' ', ' ', ' ',
};

// --- UF2 blocks ---

#define BOOT_UF2_MAGIC_START0      0x0A324655u
#define BOOT_UF2_MAGIC_START1      0x9E5D5157u
#define BOOT_UF2_MAGIC_END         0x0AB16F30u
#define BOOT_UF2_FLAG_NOT_MAIN     0x00000001u
#define BOOT_UF2_FLAG_FAMILY       0x00002000u
#define BOOT_UF2_PAYLOAD_LEN       256u
#if PICO_RP2040
#define BOOT_UF2_FAMILY            0xE48BFF56u // RP2040
#else
#define BOOT_UF2_FAMILY            0xE48BFF59u // RP2350, Arm secure (the SDK's default)
#endif
// A file can't hold more blocks than the flash has pages
#define BOOT_UF2_BLOCKS_MAX        (PICO_FLASH_SIZE_BYTES / BOOT_UF2_PAYLOAD_LEN)
#define BOOT_UF2_SECTORS_MAX       (PICO_FLASH_SIZE_BYTES / FLASH_SECTOR_SIZE)
// The host gets the last block's status before the chip restarts
#define BOOT_UF2_RESTART_MS        500u
#define BOOT_UF2_REFUSED_SHOWN_MS  5000u
#define BOOT_UF2_SCSI_SYNC_CACHE   0x35u // SYNCHRONIZE CACHE (10), which TinyUSB leaves to us

typedef struct
{
    uint32_t magic_start0;
    uint32_t magic_start1;
    uint32_t flags;
    uint32_t target_addr;
    uint32_t payload_size;
    uint32_t block_no;
    uint32_t num_blocks;
    uint32_t family_id;
    uint8_t  data[476];
    uint32_t magic_end;
} boot_uf2_block_s;

_Static_assert(sizeof(boot_uf2_block_s) == BOOT_UF2_SECTOR_LEN, "a UF2 block is one sector");
_Static_assert(BOOT_UF2_PAYLOAD_LEN == FLASH_PAGE_SIZE, "a UF2 block programs one page");

// The file being dropped. A file is told apart from the last by its block count, or by its first
// block coming again (hosts write a file in order).
static uint32_t _boot_uf2_num_blocks;
static uint32_t _boot_uf2_received;
static bool _boot_uf2_identified; // Its identity block named this board
static bool _boot_uf2_started;    // The firmware is being replaced (its first sector is gone)
static bool _boot_uf2_refused;
static uint32_t _boot_uf2_refused_at_ms;
static uint8_t _boot_uf2_block_seen[BOOT_UF2_BLOCKS_MAX / 8u];
static uint8_t _boot_uf2_sector_erased[BOOT_UF2_SECTORS_MAX / 8u];

static bool _boot_uf2_bit(const uint8_t *bits, uint32_t index)
{
    return bits[index / 8u] & (1u << (index % 8u));
}

static void _boot_uf2_set_bit(uint8_t *bits, uint32_t index)
{
    bits[index / 8u] |= (uint8_t)(1u << (index % 8u));
}

static void _boot_uf2_new_file(uint32_t num_blocks)
{
    _boot_uf2_num_blocks = num_blocks;
    _boot_uf2_received = 0;
    _boot_uf2_identified = false;
    _boot_uf2_started = false;
    _boot_uf2_refused = false;
    memset(_boot_uf2_block_seen, 0, sizeof(_boot_uf2_block_seen));
    memset(_boot_uf2_sector_erased, 0, sizeof(_boot_uf2_sector_erased));
}

static void _boot_uf2_refuse(void)
{
    _boot_uf2_refused = true;
    _boot_uf2_refused_at_ms = to_ms_since_boot(get_absolute_time());
}

static void _boot_uf2_check_identity(const uint8_t *page)
{
    boot_identity_s identity;
    memcpy(&identity, page, sizeof(identity));

    identity.build_id[BOOT_IDENTITY_BUILD_ID_LEN - 1u] = '\0';
    if ((identity.magic == BOOT_IDENTITY_MAGIC) && (identity.magic_inv == ~BOOT_IDENTITY_MAGIC) &&
        !strcmp(identity.build_id, BOOT_BUILD_ID))
        _boot_uf2_identified = true;
    else
        _boot_uf2_refuse();
}

// Into flash: the firmware and the radio region, never the bootloader, the stage header or the top
// area (settings and pairings survive, as with any update)
static void _boot_uf2_write_page(uint32_t offset, const uint8_t *data)
{
    if ((offset < BOOT_LAYOUT_BOOT_SIZE) || (offset >= BOOT_LAYOUT_STAGE_HEADER_OFFSET))
        return;

    if (!_boot_uf2_started)
    {
        // Away with the old firmware's vector table first: a copy cut short then leaves nothing to
        // start, and the bootloader stays in the update mode
        boot_usb_erase_sector(BOOT_LAYOUT_APP_OFFSET);
        _boot_uf2_set_bit(_boot_uf2_sector_erased, BOOT_LAYOUT_APP_OFFSET / FLASH_SECTOR_SIZE);
        _boot_uf2_started = true;
    }

    uint32_t sector = offset / FLASH_SECTOR_SIZE;
    if (!_boot_uf2_bit(_boot_uf2_sector_erased, sector))
    {
        boot_usb_erase_sector(sector * FLASH_SECTOR_SIZE);
        _boot_uf2_set_bit(_boot_uf2_sector_erased, sector);
    }
    boot_usb_program_page(offset, data);
}

static void _boot_uf2_block(const boot_uf2_block_s *block)
{
    if ((block->magic_start0 != BOOT_UF2_MAGIC_START0) || (block->magic_start1 != BOOT_UF2_MAGIC_START1) ||
        (block->magic_end != BOOT_UF2_MAGIC_END))
        return;

    // Another chip's blocks, or ones not for the flash, are no part of this board's image
    if ((block->flags & BOOT_UF2_FLAG_NOT_MAIN) || !(block->flags & BOOT_UF2_FLAG_FAMILY) ||
        (block->family_id != BOOT_UF2_FAMILY))
        return;

    if ((block->payload_size != BOOT_UF2_PAYLOAD_LEN) || (block->target_addr % BOOT_UF2_PAYLOAD_LEN) ||
        (block->target_addr < XIP_BASE) ||
        (block->target_addr - XIP_BASE > PICO_FLASH_SIZE_BYTES - BOOT_UF2_PAYLOAD_LEN))
        return;

    if ((block->num_blocks == 0) || (block->num_blocks > BOOT_UF2_BLOCKS_MAX) ||
        (block->block_no >= block->num_blocks))
        return;

    if ((block->num_blocks != _boot_uf2_num_blocks) || ((block->block_no == 0) && _boot_uf2_received))
        _boot_uf2_new_file(block->num_blocks);

    if (_boot_uf2_refused)
    {
        _boot_uf2_refuse(); // Still arriving: keep the LEDs on it
        return;
    }

    uint32_t offset = block->target_addr - XIP_BASE;
    if (offset == BOOT_IDENTITY_OFFSET)
    {
        _boot_uf2_check_identity(block->data);
        if (_boot_uf2_refused)
            return;
    }
    else if (offset >= BOOT_LAYOUT_BOOT_SIZE)
    {
        // The identity comes before any of the firmware: without it, the file isn't this board's
        if (!_boot_uf2_identified)
        {
            _boot_uf2_refuse();
            return;
        }
        _boot_uf2_write_page(offset, block->data);
    }

    if (!_boot_uf2_bit(_boot_uf2_block_seen, block->block_no))
    {
        _boot_uf2_set_bit(_boot_uf2_block_seen, block->block_no);
        _boot_uf2_received++;
    }

    if (_boot_uf2_started && (_boot_uf2_received == _boot_uf2_num_blocks))
        boot_usb_restart_after(BOOT_UF2_RESTART_MS);
}

bool boot_uf2_refused(uint32_t now_ms)
{
    return _boot_uf2_refused && (now_ms - _boot_uf2_refused_at_ms < BOOT_UF2_REFUSED_SHOWN_MS);
}

// --- Sectors as the host sees them ---

static void _boot_uf2_read_fat(uint32_t fat_sector, uint8_t *out)
{
    if (fat_sector != 0)
        return;

    // Media and end-of-chain marks, then one cluster per file, each the end of its chain
    uint16_t *fat = (uint16_t *)out;
    fat[0] = 0xFFF8u;
    fat[1] = 0xFFFFu;
    for (uint32_t i = 0; i < BOOT_UF2_FILE_COUNT; i++)
        fat[2u + i] = 0xFFFFu;
}

static void _boot_uf2_read_root(uint32_t root_sector, uint8_t *out)
{
    if (root_sector != 0)
        return;

    boot_uf2_dirent_s *entry = (boot_uf2_dirent_s *)out;
    memcpy(entry->name, BOOT_UF2_LABEL, sizeof(entry->name));
    entry->attr = BOOT_UF2_ATTR_LABEL;

    for (uint32_t i = 0; i < BOOT_UF2_FILE_COUNT; i++)
    {
        entry++;
        memcpy(entry->name, _boot_uf2_files[i].name, sizeof(entry->name));
        entry->attr = BOOT_UF2_ATTR_READ_ONLY;
        entry->cluster = (uint16_t)(2u + i);
        entry->size = _boot_uf2_files[i].len;
    }
}

static void _boot_uf2_read_sector(uint32_t lba, uint8_t *out)
{
    memset(out, 0, BOOT_UF2_SECTOR_LEN);

    if (lba == 0)
    {
        memcpy(out, _boot_uf2_boot_sector, sizeof(_boot_uf2_boot_sector));
        out[510] = 0x55;
        out[511] = 0xAA;
    }
    else if (lba < BOOT_UF2_ROOT_START)
    {
        _boot_uf2_read_fat((lba - BOOT_UF2_FAT_START) % BOOT_UF2_FAT_SECTORS, out);
    }
    else if (lba < BOOT_UF2_DATA_START)
    {
        _boot_uf2_read_root(lba - BOOT_UF2_ROOT_START, out);
    }
    else if (lba - BOOT_UF2_DATA_START < BOOT_UF2_FILE_COUNT)
    {
        const boot_uf2_file_s *file = &_boot_uf2_files[lba - BOOT_UF2_DATA_START];
        memcpy(out, file->text, file->len);
    }
}

// --- TinyUSB's MSC callbacks ---

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    memcpy(vendor_id, "HHL     ", 8);
    memcpy(product_id, "HOJA Update     ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;
    *block_count = BOOT_UF2_SECTORS;
    *block_size = BOOT_UF2_SECTOR_LEN;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void)lun;
    (void)power_condition;
    (void)start;
    (void)load_eject;
    return true;
}

// Whole sectors: the endpoint buffer is one (CFG_TUD_MSC_EP_BUFSIZE)
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void)lun;
    if (offset || (bufsize % BOOT_UF2_SECTOR_LEN))
        return -1;

    uint8_t *out = buffer;
    for (uint32_t done = 0; done < bufsize; done += BOOT_UF2_SECTOR_LEN, lba++)
    {
        if (lba >= BOOT_UF2_SECTORS)
            return -1;
        _boot_uf2_read_sector(lba, &out[done]);
    }
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    (void)lun;
    (void)lba;
    if (offset || (bufsize % BOOT_UF2_SECTOR_LEN))
        return -1;

    // Wherever the host puts them: a UF2 block is known by its magics, and the rest (the FAT and
    // directory the host keeps up to date) is let go
    for (uint32_t done = 0; done < bufsize; done += BOOT_UF2_SECTOR_LEN)
    {
        boot_uf2_block_s block;
        memcpy(&block, &buffer[done], sizeof(block));
        _boot_uf2_block(&block);
    }
    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    (void)buffer;
    (void)bufsize;

    // Nothing is cached, so there is nothing to flush
    if (scsi_cmd[0] == BOOT_UF2_SCSI_SYNC_CACHE)
        return 0;

    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // Invalid command
    return -1;
}
