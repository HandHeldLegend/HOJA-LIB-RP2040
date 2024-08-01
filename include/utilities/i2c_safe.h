#ifndef I2C_SAFE_H
#define I2C_SAFE_H
#include "hoja_includes.h"

int i2c_safe_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop, uint timeout_us);
int i2c_safe_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop, uint timeout_us);
int i2c_safe_write_blocking(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop);
int i2c_safe_write_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t wr_len, uint8_t *dst, size_t dst_len, uint timeout_us);

#endif