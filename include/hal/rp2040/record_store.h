#ifndef HAL_RP2040_RECORD_STORE_H
#define HAL_RP2040_RECORD_STORE_H

// Power-safe records in the top area's pool (boards with the HOJA bootloader, boot_layout.h). A
// record fills one sector and is written whole to a sector that holds no current record, with a
// sequence number; on reading, each id's newest whole copy wins. A write cut short leaves the copy
// before it.

#include <stdbool.h>
#include <stdint.h>

#define RECORD_STORE_IDS        8u
#define RECORD_STORE_HEADER_LEN 20u
#define RECORD_STORE_PAYLOAD_MAX (4096u - RECORD_STORE_HEADER_LEN)

// Scans the pool. Reading only: safe from either core.
void record_store_init(void);

// An id's newest copy in flash, or NULL
const uint8_t *record_store_find(uint16_t id, uint32_t *len);

// A new copy of an id, from two parts back to back (b may be NULL). From the core the task loop runs
// on: the other core waits out the write.
bool record_store_write(uint16_t id, const void *a, uint32_t a_len, const void *b, uint32_t b_len);

#endif
