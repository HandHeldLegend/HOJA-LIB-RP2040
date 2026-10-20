#ifndef BOOTLOADER_BOOT_UF2_H
#define BOOTLOADER_BOOT_UF2_H

// The update mode's UF2 drive: a made-up FAT volume the host can drop a board's .uf2 file onto,
// for hosts without WebUSB. The blocks of a file for this board are written as they arrive, and the
// chip restarts into the new firmware once the last one is in. A file for another board, or one
// built before images carried their identity, is refused and nothing is written.

#include <stdbool.h>
#include <stdint.h>

// True for a few seconds after a refused file (the LEDs say so)
bool boot_uf2_refused(uint32_t now_ms);

#endif
