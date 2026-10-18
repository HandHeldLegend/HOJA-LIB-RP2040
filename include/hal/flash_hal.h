#ifndef HOJA_FLASH_HAL_H
#define HOJA_FLASH_HAL_H

// HAL for data storage and retrieval
#include <stdint.h>
#include <stdbool.h>

void flash_hal_task(uint64_t now_us);
void flash_hal_init();
bool flash_hal_write(uint8_t *data, uint32_t size, uint32_t page);
bool flash_hal_read(uint8_t *out, uint32_t size, uint32_t page);

// Erases the flash sector at offset (from the start of flash) and programs len bytes of data into it:
// whole pages, 0 to erase only. data must be in RAM. The other core waits out the write once it
// stops, which it has lockout_ms to do. False when the write didn't happen.
bool flash_hal_write_sector(uint32_t offset, const uint8_t *data, uint32_t len, uint32_t lockout_ms);

#endif