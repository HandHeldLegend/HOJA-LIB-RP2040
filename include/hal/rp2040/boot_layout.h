#ifndef HAL_RP2040_BOOT_LAYOUT_H
#define HAL_RP2040_BOOT_LAYOUT_H

// Flash layout of boards that run the HOJA bootloader (HOJA_BOOTLOADER), shared by the bootloader
// and the firmware. From the start of flash:
//
//   bootloader | radio firmware | firmware ... staged update | stage header | top area
//
// - The radio region holds the CYW43's firmware at fixed addresses (boards with one): it never
//   changes between our builds, so updates leave it where it is. Its header names what is there.
// - The firmware (application) runs from just after it.
// - An update received while the firmware runs is staged in the free space below the stage header,
//   past the running firmware, and the bootloader writes it into place on the next boot: a link
//   that drops part way leaves the running firmware untouched. An update may have several parts
//   (new radio firmware with the firmware that needs it), written together in one boot.
// - The top area holds settings, profiles, a reserved Mii page and the Bluetooth pairings, counted
//   down from the end of flash.
//
// The build passes the board's sizes (hoja_use_bootloader); everything here derives from them.

#include <stdbool.h>
#include <stdint.h>

#include "hardware/flash.h"
#include "hardware/structs/watchdog.h"

#if !defined(HOJA_BOOT_FLASH_SIZE_BYTES) || !defined(HOJA_BOOT_RADIO_SIZE) || !defined(HOJA_BOOT_SIZE) || \
    !defined(HOJA_BOOT_TOP_SIZE) || !defined(HOJA_BOOT_STAGE_HEADER_SIZE)
#error "boot_layout.h: built without hoja_use_bootloader"
#endif

#if HOJA_BOOT_FLASH_SIZE_BYTES != PICO_FLASH_SIZE_BYTES
#error "hoja_use_bootloader's flash size differs from PICO_FLASH_SIZE_BYTES"
#endif

#define BOOT_LAYOUT_BOOT_SIZE           HOJA_BOOT_SIZE
#define BOOT_LAYOUT_TOP_SIZE            HOJA_BOOT_TOP_SIZE
#define BOOT_LAYOUT_STAGE_HEADER_SIZE   HOJA_BOOT_STAGE_HEADER_SIZE

#define BOOT_LAYOUT_RADIO_OFFSET        BOOT_LAYOUT_BOOT_SIZE
#define BOOT_LAYOUT_RADIO_SIZE          HOJA_BOOT_RADIO_SIZE
#define BOOT_LAYOUT_APP_OFFSET          (BOOT_LAYOUT_RADIO_OFFSET + BOOT_LAYOUT_RADIO_SIZE)
#define BOOT_LAYOUT_TOP_OFFSET          (PICO_FLASH_SIZE_BYTES - BOOT_LAYOUT_TOP_SIZE)
#define BOOT_LAYOUT_STAGE_HEADER_OFFSET (BOOT_LAYOUT_TOP_OFFSET - BOOT_LAYOUT_STAGE_HEADER_SIZE)
// The firmware may fill everything up to the stage header (written over USB). Staging an update
// needs its own room as well, below the header and above both the old and the new firmware.
#define BOOT_LAYOUT_APP_MAX             (BOOT_LAYOUT_STAGE_HEADER_OFFSET - BOOT_LAYOUT_APP_OFFSET)

// An RP2040 application image starts with its own copy of the second stage (unused: the
// bootloader's ran), so it is laid out as any Pico image and its vector table follows it. An RP2350
// image has none: its vector table comes first.
#if PICO_RP2040
#define BOOT_LAYOUT_BOOT2_SIZE          0x100u
#else
#define BOOT_LAYOUT_BOOT2_SIZE          0u
#endif
#define BOOT_LAYOUT_APP_VECTORS_OFFSET  (BOOT_LAYOUT_APP_OFFSET + BOOT_LAYOUT_BOOT2_SIZE)

// Every region is erased and written in whole sectors, and the stage header is one of them
_Static_assert((BOOT_LAYOUT_BOOT_SIZE % FLASH_SECTOR_SIZE) == 0, "bootloader size not whole sectors");
_Static_assert((BOOT_LAYOUT_RADIO_SIZE % FLASH_SECTOR_SIZE) == 0, "radio region not whole sectors");
_Static_assert((BOOT_LAYOUT_TOP_SIZE % FLASH_SECTOR_SIZE) == 0, "top area not whole sectors");
_Static_assert(BOOT_LAYOUT_STAGE_HEADER_SIZE == FLASH_SECTOR_SIZE, "the stage header is one sector");

// Rounded up to whole sectors. Up to a sector short of 4 GB: past that it wraps to 0.
static inline uint32_t boot_layout_sector_align(uint32_t len)
{
    return ((len + FLASH_SECTOR_SIZE - 1u) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
}

// --- Staged update ---

// The header sector, written last, once the data below it is complete: magic and its inverse mark
// a whole header, the CRCs each part's data as staged and its image once in place. An update never
// replaces the bootloader, so a new firmware must stage the format an installed bootloader reads.
#define BOOT_STAGE_MAGIC     0x45474154u // "TAGE"
#define BOOT_STAGE_VERSION   1u
#define BOOT_STAGE_PARTS_MAX 4u

typedef enum
{
    BOOT_STAGE_STORED  = 0, // The image as it is
    BOOT_STAGE_DEFLATE = 1, // Raw deflate (RFC 1951), as a browser's CompressionStream("deflate-raw")
} boot_stage_encoding_t;

typedef enum
{
    BOOT_STAGE_TO_APP   = 0,
    BOOT_STAGE_TO_RADIO = 1,
} boot_stage_destination_t;

// One part. Its staged data lies wholly above every destination the update writes, below the
// header, apart from the other parts' data; each destination is written at most once.
typedef struct
{
    uint8_t  destination;   // boot_stage_destination_t
    uint8_t  encoding;      // boot_stage_encoding_t
    uint8_t  reserved[2];
    uint32_t staged_offset; // From the start of flash
    uint32_t staged_size;
    uint32_t staged_crc32;
    uint32_t image_size;    // Bytes written from the start of the destination
    uint32_t image_crc32;   // Reflected 0xEDB88320, as the config app's block fingerprints
} boot_stage_part_s;

typedef struct
{
    uint32_t magic;
    uint32_t magic_inv;
    uint8_t  version;       // BOOT_STAGE_VERSION
    uint8_t  part_count;
    uint8_t  reserved[2];
    boot_stage_part_s parts[BOOT_STAGE_PARTS_MAX];
} boot_stage_header_s;

_Static_assert(sizeof(boot_stage_part_s) == 24u, "boot_stage_part_s is an on-flash format");
_Static_assert(sizeof(boot_stage_header_s) == 108u, "boot_stage_header_s is an on-flash format");
// The firmware programs the header as one page
_Static_assert(sizeof(boot_stage_header_s) <= FLASH_PAGE_SIZE, "the stage header outgrew a page");

// Where a destination lies in flash
typedef struct
{
    uint32_t offset; // From the start of flash
    uint32_t room;   // Bytes
} boot_stage_region_s;

// Why a header can't be written into place (boot_stage_header_check)
typedef enum
{
    BOOT_STAGE_VALID = 0,
    BOOT_STAGE_INVALID, // Not a whole header, or a part this board can't take
    BOOT_STAGE_NO_ROOM, // Staged data outside the stage area, or where the update writes
} boot_stage_check_t;

// False, and no room, for a destination this board doesn't have
static inline bool boot_stage_region(uint8_t destination, boot_stage_region_s *region)
{
    switch (destination)
    {
        case BOOT_STAGE_TO_APP:
            region->offset = BOOT_LAYOUT_APP_OFFSET;
            region->room = BOOT_LAYOUT_APP_MAX;
            return true;

        case BOOT_STAGE_TO_RADIO:
            region->offset = BOOT_LAYOUT_RADIO_OFFSET;
            region->room = BOOT_LAYOUT_RADIO_SIZE;
            return BOOT_LAYOUT_RADIO_SIZE > 0;

        default:
            region->offset = 0;
            region->room = 0;
            return false;
    }
}

static inline bool boot_stage_header_whole(const boot_stage_header_s *header)
{
    return (header->magic == BOOT_STAGE_MAGIC) && (header->magic_inv == ~BOOT_STAGE_MAGIC) &&
           (header->version == BOOT_STAGE_VERSION) && (header->part_count >= 1u) &&
           (header->part_count <= BOOT_STAGE_PARTS_MAX);
}

// A part on its own, wherever it is staged: a destination the board has, an image that fits it,
// and an encoding the bootloader reads
static inline bool boot_stage_part_valid(const boot_stage_part_s *part)
{
    boot_stage_region_s region;

    if (!boot_stage_region(part->destination, &region))
        return false;

    if ((part->image_size == 0) || (part->image_size > region.room) || (part->staged_size == 0))
        return false;

    if (part->encoding == BOOT_STAGE_STORED)
        return part->staged_size == part->image_size;

    return part->encoding == BOOT_STAGE_DEFLATE;
}

// The bootloader writes a header only when this is BOOT_STAGE_VALID: each destination once, and no
// write lands on staged data still to be read (each part's data lies above every destination's
// written end, below the header and apart from the other parts' data)
static inline boot_stage_check_t boot_stage_header_check(const boot_stage_header_s *header)
{
    uint32_t written_end = 0;

    if (!boot_stage_header_whole(header))
        return BOOT_STAGE_INVALID;

    for (uint8_t i = 0; i < header->part_count; i++)
    {
        const boot_stage_part_s *part = &header->parts[i];
        boot_stage_region_s region;

        if (!boot_stage_part_valid(part))
            return BOOT_STAGE_INVALID;

        for (uint8_t j = 0; j < i; j++)
        {
            if (header->parts[j].destination == part->destination)
                return BOOT_STAGE_INVALID;
        }

        // Written in whole sectors, the last one padded
        boot_stage_region(part->destination, &region);
        uint32_t end = boot_layout_sector_align(region.offset + part->image_size);
        if (end > written_end)
            written_end = end;
    }

    for (uint8_t i = 0; i < header->part_count; i++)
    {
        const boot_stage_part_s *part = &header->parts[i];

        if ((part->staged_offset < BOOT_LAYOUT_APP_OFFSET) || (part->staged_offset < written_end) ||
            (part->staged_offset > BOOT_LAYOUT_STAGE_HEADER_OFFSET) ||
            (part->staged_size > BOOT_LAYOUT_STAGE_HEADER_OFFSET - part->staged_offset))
            return BOOT_STAGE_NO_ROOM;

        // The earlier parts passed the check above, so these sums can't wrap
        for (uint8_t j = 0; j < i; j++)
        {
            const boot_stage_part_s *other = &header->parts[j];
            if ((part->staged_offset < other->staged_offset + other->staged_size) &&
                (other->staged_offset < part->staged_offset + part->staged_size))
                return BOOT_STAGE_NO_ROOM;
        }
    }
    return BOOT_STAGE_VALID;
}

// --- Radio region ---

// The radio region's first bytes, and the firmware's own copy of them: the firmware starts the
// radio only when the two match (the region holds the radio firmware it was linked against)
#define BOOT_RADIO_MAGIC   0x4F494452u // "RDIO"
#define BOOT_RADIO_VERSION 1u
#define BOOT_RADIO_TAG_LEN 48u

typedef struct
{
    uint32_t magic;
    uint32_t version;
    char     tag[BOOT_RADIO_TAG_LEN]; // The firmware's file, as the radio driver names it
    uint32_t parts[4];                // Start and end address of the Wi-Fi/CLM blob, then of the BT patch
} boot_radio_header_s;

// The config app compares a new image's radio header with this one's CRC (boot_update_info)
_Static_assert(sizeof(boot_radio_header_s) == 72u, "boot_radio_header_s is an on-flash format");

// --- Requests ---

// Requests across a restart, in two watchdog scratch registers (the value and its inverse), which
// survive the watchdog reboot (HOJA's boot memory uses register 5, the SDK 4 to 7). The bootloader
// takes only its own and leaves any other for the firmware.
#define BOOT_REQUEST_SCRATCH_IDX 0
#define BOOT_REQUEST_UPDATE      0x44505548u // "HUPD": the firmware asks for the USB update mode
#define BOOT_REQUEST_BEACON      0x4E434248u // "HBCN": open the config beacon (the app reconnects)

// Leaves a request for the next boot; the caller then restarts through the watchdog
static inline void boot_request_set(uint32_t request)
{
    watchdog_hw->scratch[BOOT_REQUEST_SCRATCH_IDX] = request;
    watchdog_hw->scratch[BOOT_REQUEST_SCRATCH_IDX + 1] = ~request;
}

// True when this request is there, and clears it so it acts once. Any other request stays.
static inline bool boot_request_take(uint32_t request)
{
    io_rw_32 *scratch = &watchdog_hw->scratch[BOOT_REQUEST_SCRATCH_IDX];

    if ((scratch[0] != request) || (scratch[1] != ~request))
        return false;

    scratch[0] = 0;
    scratch[1] = 0;
    return true;
}

// The bootloader's last fault, kept across the restart it causes: the panic message's address, or
// BOOT_FAULT_HARD and the faulting PC. The update mode reports them (boot_usb.c).
#define BOOT_FAULT_SCRATCH_IDX 2
#define BOOT_FAULT_HARD        0xFA017000u

#endif
