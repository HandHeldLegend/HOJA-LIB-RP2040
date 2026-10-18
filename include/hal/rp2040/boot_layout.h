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
// - The top area holds settings, profiles, Mii data and the Bluetooth pairings, counted down from
//   the end of flash.
//
// The build passes the board's sizes (hoja_use_bootloader); everything here derives from them.

#include <stdint.h>

#include "hardware/flash.h"

#if !defined(HOJA_BOOT_FLASH_SIZE_BYTES) || !defined(HOJA_BOOT_RADIO_SIZE)
#error "boot_layout.h: built without hoja_use_bootloader"
#endif

#if HOJA_BOOT_FLASH_SIZE_BYTES != PICO_FLASH_SIZE_BYTES
#error "hoja_use_bootloader's flash size differs from PICO_FLASH_SIZE_BYTES"
#endif

// These match HOJA_BOOT_SIZE and HOJA_BOOT_TOP_SIZE in CMakeLists.txt
#define BOOT_LAYOUT_BOOT_SIZE      (32u * 1024u)
#define BOOT_LAYOUT_TOP_SIZE       (64u * 1024u)

#define BOOT_LAYOUT_RADIO_OFFSET   BOOT_LAYOUT_BOOT_SIZE
#define BOOT_LAYOUT_RADIO_SIZE     HOJA_BOOT_RADIO_SIZE
#define BOOT_LAYOUT_APP_OFFSET     (BOOT_LAYOUT_RADIO_OFFSET + BOOT_LAYOUT_RADIO_SIZE)
#define BOOT_LAYOUT_TOP_OFFSET     (PICO_FLASH_SIZE_BYTES - BOOT_LAYOUT_TOP_SIZE)
#define BOOT_LAYOUT_STAGE_HEADER_OFFSET (BOOT_LAYOUT_TOP_OFFSET - FLASH_SECTOR_SIZE)
// The firmware may fill everything up to the stage header (written over USB). Staging an update
// needs its own room as well, below the header and above both the old and the new firmware.
#define BOOT_LAYOUT_APP_MAX        (BOOT_LAYOUT_STAGE_HEADER_OFFSET - BOOT_LAYOUT_APP_OFFSET)

// An application image starts with its own copy of the second stage (unused: the bootloader's ran),
// so it is laid out as any Pico image and its vector table follows it
#define BOOT_LAYOUT_APP_VECTORS_OFFSET (BOOT_LAYOUT_APP_OFFSET + 0x100u)

// --- Staged update ---

// The header sector, written last, once the data below it is complete: magic and its inverse mark
// a whole header, the CRCs each part's data as staged and its image once in place
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
    uint32_t parts[4];                // Address and length of the Wi-Fi/CLM blob, then the BT patch
} boot_radio_header_s;

// --- Requests ---

// Requests across a restart, in two watchdog scratch registers (the value and its inverse), which
// survive the watchdog reboot (HOJA's boot memory uses register 5, the SDK 4 to 7). The bootloader
// takes only its own and leaves any other for the firmware.
#define BOOT_REQUEST_SCRATCH_IDX 0
#define BOOT_REQUEST_UPDATE      0x44505548u // "HUPD": the firmware asks for the USB update mode
#define BOOT_REQUEST_BEACON      0x4E434248u // "HBCN": open the config beacon (the app reconnects)

// The bootloader's last fault, kept across the restart it causes: the panic message's address, or
// BOOT_FAULT_HARD and the faulting PC. The update mode reports them (BOOT_REQUEST_FAULT).
#define BOOT_FAULT_SCRATCH_IDX 2
#define BOOT_FAULT_HARD        0xFA017000u

// The update mode on USB. The HOJA Gamepad IDs, told apart from a running gamepad by the device
// version (0xB0 in its high byte); its serial is the build id (a separate device to Windows, the board
// to the app), and its MS OS 2.0 descriptors bind WinUSB.
#define BOOT_USB_VID 0x2E8Au
#define BOOT_USB_PID 0x10C6u
#define BOOT_USB_BCD 0xB001u

// Commands beyond the RP2040 boot ROM's PICOBOOT set
#define BOOT_CMD_ROM_BOOTLOADER 0x21u // Restart into the boot ROM's own USB bootloader
// Interface requests beside PICOBOOT's
#define BOOT_REQUEST_FAULT      0x48u // The last fault, 8 bytes (see BOOT_FAULT_SCRATCH_IDX)
#define BOOT_REQUEST_LAYOUT     0x49u // boot_layout_report_s

// The layout as the update mode reports it, so tools need no board tables
typedef struct __attribute__((packed))
{
    uint8_t  version;            // BOOT_STAGE_VERSION
    uint8_t  reserved[3];
    uint32_t flash_size;
    uint32_t radio_offset;
    uint32_t radio_size;
    uint32_t app_offset;
    uint32_t stage_header_offset;
    uint32_t top_offset;
} boot_layout_report_s;

#endif
