#ifndef HAL_RECORD_HAL_H
#define HAL_RECORD_HAL_H

// Power-safe records: a few small blobs kept by id in their own area of flash. A record fills one
// sector and is written whole, with a sequence number, to a sector that holds no id's newest copy; on
// reading, each id's newest whole copy wins, so a write cut short leaves the copy before it. Boards
// with the HOJA bootloader keep them in its top area (rp2040/record_hal.c). Other boards have none:
// record_hal_init() is false and every write fails.

#include <stdbool.h>
#include <stdint.h>

#define RECORD_HAL_IDS             8u
#define RECORD_HAL_SECTOR_LEN      4096u // One record per flash sector
#define RECORD_HAL_HEADER_LEN      20u
#define RECORD_HAL_PAYLOAD_MAX_LEN (RECORD_HAL_SECTOR_LEN - RECORD_HAL_HEADER_LEN)
#define RECORD_HAL_PARTS_MAX       8u // Parts in one write

// One piece of a record's payload; a record is written from several, back to back
typedef struct
{
    const void *data;
    uint32_t len;
} record_hal_part_s;

// Scans the store. False without one. Reading only: safe from either core.
bool record_hal_init(void);

// An id's newest copy, or NULL. The pointer is into flash (XIP): valid until a later write, which may
// reuse its sector.
const uint8_t *record_hal_find(uint16_t id, uint32_t *len);

// A new copy of an id, from parts back to back. A part may point into the store (an id's newest copy):
// every part is read before anything is written. From the core the task loop runs on: the other core
// waits out the write. False when no whole copy could be written (the copy before stays).
bool record_hal_write(uint16_t id, const record_hal_part_s *parts, uint8_t count);

#endif
