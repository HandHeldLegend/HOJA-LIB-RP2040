#ifndef HAL_RP2040_BOOT_UPDATE_H
#define HAL_RP2040_BOOT_UPDATE_H

// Receiving an update while the firmware runs (boards with the HOJA bootloader, boot_layout.h). It
// is staged below the stage header, past this firmware, and the bootloader writes it into place on
// the next boot; until the header is written last, nothing the bootloader acts on has changed.
//
// The sender streams the parts' staged data back to back, each padded to a whole sector (4 KB of
// 0xFF), in order and at most BOOT_UPDATE_WINDOW bytes past what is written; it hears the written
// count (boot_update_written) as acknowledgement, and may resend from any earlier point (what is
// here already is skipped). A link that drops leaves the update where it got to: beginning the same
// update again carries on from what is written. Boards without the bootloader answer
// BOOT_UPDATE_UNSUPPORTED to everything.

#include <stdbool.h>
#include <stdint.h>

#define BOOT_UPDATE_WINDOW    (2u * 4096u)
#define BOOT_UPDATE_PARTS_MAX 2u
// The battery an update needs, unless the controller is plugged in: the first boot after one
// writes for a few seconds and must not run out part way (it would start again, but on what?)
#define BOOT_UPDATE_BATTERY_MIN_PERCENT 30u

typedef enum
{
    BOOT_UPDATE_OK = 0,
    BOOT_UPDATE_UNSUPPORTED,
    BOOT_UPDATE_BAD_REQUEST,  // Sizes, parts or encodings this layout can't take
    BOOT_UPDATE_NO_ROOM,      // Not enough free flash between this firmware and the stage header
    BOOT_UPDATE_BATTERY_LOW,
    BOOT_UPDATE_OUT_OF_ORDER, // Data not where the stream stands, or past the window: resend from written
    BOOT_UPDATE_FAILED,       // A staged part's CRC didn't match (or no update under way)
    BOOT_UPDATE_RESTARTING,   // Staged: the controller restarts to write it
} boot_update_status_t;

typedef struct
{
    uint8_t  destination; // boot_stage_destination_t
    uint8_t  encoding;    // boot_stage_encoding_t
    uint32_t staged_size;
    uint32_t staged_crc32;
    uint32_t image_size;
    uint32_t image_crc32;
} boot_update_part_s;

typedef struct
{
    bool     supported;
    bool     battery_ok;
    uint8_t  battery_percent;   // 0xFF without a fuel gauge
    uint32_t app_offset;        // Where the firmware starts in a whole board image
    uint32_t radio_offset;
    uint32_t radio_size;        // 0: no radio region
    uint32_t stage_header_offset;
    uint32_t running_end;       // The end of this firmware in flash
    uint32_t radio_header_crc32; // Of the radio region's header, to compare with a new image's
} boot_update_info_s;

void boot_update_info(boot_update_info_s *info);

boot_update_status_t boot_update_begin(const boot_update_part_s *parts, uint8_t count);

// From the transport's receive path: copies only (the task writes)
boot_update_status_t boot_update_data(uint32_t offset, const uint8_t *data, uint32_t len);

// Once everything is written (boot_update_written equals the stream's size): checks the staged
// parts, writes the header and, on BOOT_UPDATE_RESTARTING, restarts shortly
boot_update_status_t boot_update_finish(void);

void boot_update_abort(void);

// From the task: writes one received sector at a time. True when boot_update_written moved.
bool boot_update_task(uint64_t now_us);

uint32_t boot_update_written(void);

bool boot_update_active(void);

// An update's restart asks the next boot to open the config beacon, so the app reconnects without
// a press. True once.
bool boot_update_take_beacon(void);

#endif
