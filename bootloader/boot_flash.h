#ifndef BOOTLOADER_BOOT_FLASH_H
#define BOOTLOADER_BOOT_FLASH_H

// Flash writes for the bootloader. Offsets from the start of flash, sector (erase) or page (program)
// aligned. Interrupts stay off for each one: their handlers run from flash, which is unreadable
// while it is written.

#include <stdint.h>

// Erases len bytes, whole sectors, in one go: a caller with a watchdog to feed erases a sector a call
void boot_flash_erase(uint32_t offset, uint32_t len);

// Programs len bytes, whole pages, into erased flash
void boot_flash_program(uint32_t offset, const uint8_t *data, uint32_t len);

// Erases one sector and programs a whole sector's worth from RAM into it
void boot_flash_write_sector(uint32_t offset, const uint8_t *sector);

#endif
