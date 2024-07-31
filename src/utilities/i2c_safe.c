#include "i2c_safe.h"

uint32_t _i2c_owner_1;
uint32_t _i2c_owner_2;
auto_init_mutex(_i2c_safe_mutex);

void i2c_safe_enter_blocking()
{
    while(!mutex_enter_timeout_us(&_i2c_safe_mutex, 10))
    {
        watchdog_update();
    }
}

void i2c_safe_exit()
{
    mutex_exit(&_i2c_safe_mutex);
}

int i2c_safe_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop, uint timeout_us)
{
    int ret = 0;
    i2c_safe_enter_blocking();
    ret = i2c_write_timeout_us(i2c, addr, src, len, nostop, timeout_us);
    i2c_safe_exit();
    return ret;
}

int i2c_safe_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop, uint timeout_us)
{
    int ret = 0;
    i2c_safe_enter_blocking();
    ret = i2c_read_timeout_us(i2c, addr, dst, len, nostop, timeout_us);
    i2c_safe_exit();
    return ret;
}

int i2c_safe_write_blocking(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop)
{
    int ret = 0;
    i2c_safe_enter_blocking();
    ret = i2c_write_blocking(i2c, addr, src, len, nostop);
    i2c_safe_exit();
    return ret;
}

// Perform a write then read without letting go of the i2c bus
int i2c_safe_write_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t wr_len, uint8_t *dst, size_t dst_len, uint timeout_us)
{
    int ret = 0;
    i2c_safe_enter_blocking();
    ret = i2c_write_timeout_us(i2c, addr, src, wr_len, true, timeout_us);
    ret = i2c_read_timeout_us(i2c, addr, dst, dst_len, false, timeout_us);
    i2c_safe_exit();
    return ret;
}