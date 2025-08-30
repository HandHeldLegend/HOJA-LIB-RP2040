#ifndef HOJA_I2C_HAL_H
#define HOJA_I2C_HAL_H

#include "hoja_bsp.h"

#if defined(HOJA_BSP_HAS_I2C) && (HOJA_BSP_HAS_I2C > 0)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool i2c_hal_init(uint8_t instance, uint32_t sda, uint32_t scl, uint32_t baudrate_khz);

void i2c_hal_deinit(uint8_t instance);

int i2c_hal_write_timeout_us_odbaud(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop, int timeout_us, uint32_t baud_khz_override);
int i2c_hal_write_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop, int timeout_us);

int i2c_hal_read_timeout_us_odbaud(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len, bool nostop, int timeout_us, uint32_t baud_khz_override);
int i2c_hal_read_timeout_us(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len, bool nostop, int timeout_us);

int i2c_hal_write_blocking(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop);

// Write then read in sequence (For reading a specific register, as an example)
int i2c_hal_write_read_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, size_t wr_len, uint8_t *dst, size_t dst_len, int timeout_us);
int i2c_hal_write_read_blocking(uint8_t instance, uint8_t addr, const uint8_t *src,
                                size_t wr_len, uint8_t *dst, size_t dst_len);

#endif
#endif