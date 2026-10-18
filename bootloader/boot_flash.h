#ifndef BOOTLOADER_BOOT_FLASH_H
#define BOOTLOADER_BOOT_FLASH_H

// Flash writes for the bootloader. Offsets from the start of flash, sector (erase) or page (program)
// aligned. Interrupts stay off for each one: their handlers run from flash, which is unreadable
// while it is written.

#include <stdbool.h>
#include <stdint.h>

void boot_flash_erase(uint32_t offset, uint32_t len);

void boot_flash_program(uint32_t offset, const uint8_t *data, uint32_t len);

// Erase one sector and program a whole sector's worth from RAM into it
void boot_flash_write_sector(uint32_t offset, const uint8_t *sector);

// Reflected CRC-32 (0xEDB88320), as the config app computes it
uint32_t boot_flash_crc32(const uint8_t *data, uint32_t len);

#endif
