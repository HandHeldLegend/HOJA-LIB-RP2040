#ifndef HOJA_I2C_HAL_H
#define HOJA_I2C_HAL_H

#include "hoja_bsp.h"

#if defined(HOJA_BSP_HAS_I2C) && (HOJA_BSP_HAS_I2C > 0)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool i2c_hal_init(uint8_t instance, uint32_t sda, uint32_t scl, uint32_t baudrate_khz);

// For a driver running its own transfers in the background (ESP32 HCI link) so the calls
// below can share the bus. Acquire waits for it and holds it off, release lets it continue.
typedef struct
{
    void (*acquire)(void);
    void (*release)(void);
} i2c_hal_bus_hooks_s;
void i2c_hal_set_bus_hooks(uint8_t instance, const i2c_hal_bus_hooks_s *hooks);

uint32_t i2c_hal_get_baudrate(uint8_t instance);

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