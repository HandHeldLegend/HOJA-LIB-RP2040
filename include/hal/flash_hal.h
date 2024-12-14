#ifndef HOJA_FLASH_HAL_H
#define HOJA_FLASH_HAL_H

// HAL for data storage and retrieval
#include <stdint.h>
#include <stdbool.h>

void flash_hal_init();
bool flash_hal_write(const uint8_t *data, uint32_t size, uint32_t page);
bool flash_hal_read(uint8_t *out, uint32_t size, uint32_t page);

#endif