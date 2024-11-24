#ifndef HOJA_SPI_HAL_H
#define HOJA_SPI_HAL_H

#include "hoja_bsp.h"

#ifdef HOJA_BSP_HAS_SPI
#if HOJA_BSP_HAS_SPI > 0

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool spi_hal_init(uint8_t instance, uint32_t clock, uint32_t miso, uint32_t mosi);

bool spi_hal_deinit(uint8_t instance);

int spi_hal_read_blocking(uint8_t instance, uint32_t cs_gpio, 
    uint8_t repeated_tx_data, uint8_t *dst, size_t len);

int spi_hal_write_blocking(uint8_t instance, uint32_t cs_gpio, const uint8_t *data, size_t len);

int spi_hal_read_write_blocking(uint8_t instance, uint32_t cs_gpio, 
    const uint8_t *data, size_t data_len, uint8_t repeated_tx_data, uint8_t *dst, size_t dst_len);

#endif
#endif

#endif