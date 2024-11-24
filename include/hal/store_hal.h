#ifndef HOJA_STORE_HAL_H
#define HOJA_STORE_HAL_H

// HAL for data storage and retrieval
#include <stdint.h>

bool store_hal_init();

bool store_hal_read_settings(int bank, uint8_t *dst);

bool store_hal_write_settings(int bank, const uint8_t *data);

void store_hal_task();

#endif