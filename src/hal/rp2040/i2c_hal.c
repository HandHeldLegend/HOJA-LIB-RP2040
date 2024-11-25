#include "hal/i2c_hal.h"
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "board_config.h"

#define I2C_HAL_MAX_INSTANCES 2

// I2C
#if defined(HOJA_I2C_0_ENABLE) && (HOJA_I2C_0_ENABLE==1)
  #ifndef HOJA_I2C_0_GPIO_SDA
    #error "HOJA_I2C_0_GPIO_SDA undefined in board_config.h" 
  #endif

  #ifndef HOJA_I2C_0_GPIO_SCL
    #error "HOJA_I2C_0_GPIO_SCL undefined in board_config.h" 
  #endif
#endif

i2c_inst_t* _i2c_instances[2] = {i2c0, i2c1}; // Numerical accessible array to spi hardware
auto_init_mutex(_i2c_safe_mutex); // Mutex to allow thread-safe access to peripheral

void _i2c_safe_enter_blocking()
{
    while(!mutex_enter_timeout_us(&_i2c_safe_mutex, 10))
    {
        sleep_us(1);
    }
}

void _i2c_safe_exit()
{
    mutex_exit(&_i2c_safe_mutex);
}

bool i2c_hal_init(uint8_t instance, uint32_t sda, uint32_t scl)
{
    if(instance>=I2C_HAL_MAX_INSTANCES) return false;

    i2c_init(_i2c_instances[instance], 400*1000);
    gpio_set_function(HOJA_I2C_0_GPIO_SDA, GPIO_FUNC_I2C);
    gpio_set_function(HOJA_I2C_0_GPIO_SCL, GPIO_FUNC_I2C);
}

void i2c_hal_deinit(uint8_t instance)
{

}

int i2c_hal_write_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop, int timeout_us)
{
    if(instance>=I2C_HAL_MAX_INSTANCES) return -1;

    _i2c_safe_enter_blocking();
    int ret = i2c_write_timeout_us(_i2c_instances[instance], addr, src, len, nostop, timeout_us);
    _i2c_safe_exit();

    return ret;
}

int i2c_hal_read_timeout_us(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len, bool nostop, int timeout_us)
{
    if(instance>=I2C_HAL_MAX_INSTANCES) return -1;

    _i2c_safe_enter_blocking();
    int ret = i2c_read_timeout_us(_i2c_instances[instance], addr, dst, len, nostop, timeout_us);
    _i2c_safe_exit();

    return ret;
}

int i2c_hal_write_blocking(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop)
{
    if(instance>=I2C_HAL_MAX_INSTANCES) return -1;

    _i2c_safe_enter_blocking();
    int ret = i2c_write_blocking(_i2c_instances[instance], addr, src, len, nostop);
    _i2c_safe_exit();

    return ret;
}

// Write then read in sequence (For reading a specific register, as an example)
int i2c_hal_write_read_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, 
                                  size_t wr_len, uint8_t *dst, size_t dst_len, int timeout_us)
{
    if(instance>=I2C_HAL_MAX_INSTANCES) return -1;

    _i2c_safe_enter_blocking();
    int ret = i2c_write_timeout_us(_i2c_instances[instance], addr, src, wr_len, true, timeout_us);
    ret = i2c_read_timeout_us(_i2c_instances[instance], addr, dst, dst_len, false, timeout_us);
    _i2c_safe_exit();

    return ret;
}


