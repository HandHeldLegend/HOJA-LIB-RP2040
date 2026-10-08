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
// Register read in one burst: the register byte and every read command go into the FIFO at once,
// so the bus never idles between bytes. Returns len, or a negative error.
#define I2C_HAL_BURST_MAX 15 // FIFO depth less the register byte
int i2c_hal_read_reg_burst_us(uint8_t instance, uint8_t addr, uint8_t reg, uint8_t *dst, size_t len, int timeout_us);

// Register bursts run in the background from the I2C interrupt, one after another, so the CPU
// can work while the bus is busy. Every other call on the instance waits for them to finish.
// Not available on an instance with bus hooks.
#define I2C_HAL_ASYNC_MAX 4
typedef struct
{
    uint8_t  addr;
    uint8_t  reg;
    uint8_t  len;
    uint8_t *dst;
} i2c_hal_burst_s;

// False when the bursts can't run in the background (the caller reads them itself)
bool i2c_hal_read_reg_burst_async(uint8_t instance, const i2c_hal_burst_s *bursts, uint8_t count);
// Waits up to timeout_us for the bursts started last. Bit n set when burst n completed.
// Returns 0 when nothing was started.
uint8_t i2c_hal_async_wait(uint8_t instance, int timeout_us);

int i2c_hal_write_read_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, size_t wr_len, uint8_t *dst, size_t dst_len, int timeout_us);
int i2c_hal_write_read_blocking(uint8_t instance, uint8_t addr, const uint8_t *src,
                                size_t wr_len, uint8_t *dst, size_t dst_len);

#endif
#endif