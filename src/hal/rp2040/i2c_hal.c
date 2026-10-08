#include "hal/i2c_hal.h"
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "pico/timeout_helper.h"
#include "hardware/gpio.h"

#define I2C_HAL_MAX_INSTANCES 2

static void _i2c_hal_drain_abort(i2c_hw_t *hw)
{
    (void) hw->clr_tx_abrt;
    if (hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS) {
        (void) hw->clr_stop_det;
    }
}

i2c_inst_t *_i2c_instances[2] = {i2c0, i2c1}; // Numerical accessible array to spi hardware
uint32_t _i2c_instances_bauds[2] = {400*1000, 400*1000}; // Baud rates defaulted to 400Khz

static const i2c_hal_bus_hooks_s *_bus_hooks[I2C_HAL_MAX_INSTANCES] = {NULL};
static uint8_t _bus_depth[I2C_HAL_MAX_INSTANCES] = {0};
static bool _bus_held_for_restart[I2C_HAL_MAX_INSTANCES] = {false};

void i2c_hal_set_bus_hooks(uint8_t instance, const i2c_hal_bus_hooks_s *hooks)
{
  if(instance < I2C_HAL_MAX_INSTANCES) _bus_hooks[instance] = hooks;
}

uint32_t i2c_hal_get_baudrate(uint8_t instance)
{
  return (instance < I2C_HAL_MAX_INSTANCES) ? _i2c_instances_bauds[instance] : 0;
}

static void _bus_enter(uint8_t instance)
{
  if(!_bus_hooks[instance]) return;
  if(_bus_depth[instance]++ == 0 && !_bus_held_for_restart[instance])
    _bus_hooks[instance]->acquire();
}

static void _bus_exit(uint8_t instance, bool nostop)
{
  if(!_bus_hooks[instance]) return;
  if(--_bus_depth[instance]) return;

  // Without a STOP the bus stays ours until the next transfer completes
  _bus_held_for_restart[instance] = nostop;
  if(!nostop) _bus_hooks[instance]->release();
}

bool i2c_hal_init(uint8_t instance, uint32_t sda, uint32_t scl, uint32_t baudrate_khz)
{
  if (instance >= I2C_HAL_MAX_INSTANCES)
    return false;

  _i2c_instances_bauds[instance] = (baudrate_khz * 1000);

  i2c_init(_i2c_instances[instance], _i2c_instances_bauds[instance]);
  gpio_set_function(sda, GPIO_FUNC_I2C);
  gpio_set_function(scl, GPIO_FUNC_I2C);

  return true;
}

void i2c_hal_deinit(uint8_t instance)
{
}

int i2c_hal_write_timeout_us_odbaud(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop, int timeout_us, uint32_t baud_khz_override)
{
  if (instance >= I2C_HAL_MAX_INSTANCES)
    return -1;
  int ret = 0;

  _bus_enter(instance);
  uint32_t baud_original = _i2c_instances_bauds[instance];
  if(baud_khz_override)
  {
    i2c_set_baudrate(_i2c_instances[instance], (baud_khz_override*1000));
  }

  ret = i2c_write_timeout_us(_i2c_instances[instance], addr, src, len, nostop, timeout_us);

  if(baud_khz_override)
  {
    i2c_set_baudrate(_i2c_instances[instance], baud_original);
  }
  _bus_exit(instance, nostop);

  return ret;
}

int i2c_hal_write_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop, int timeout_us)
{
  return i2c_hal_write_timeout_us_odbaud(instance, addr, src, len, nostop, timeout_us, 0);
}

int i2c_hal_read_timeout_us_odbaud(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len, bool nostop, int timeout_us, uint32_t baud_khz_override)
{
  if (instance >= I2C_HAL_MAX_INSTANCES)
    return -1;
  int ret = 0;

  _bus_enter(instance);
  uint32_t baud_original = _i2c_instances_bauds[instance];
  if(baud_khz_override)
  {
    i2c_set_baudrate(_i2c_instances[instance], (baud_khz_override*1000));
  }

  //ret = _i2c_hal_read_timeout_us(_i2c_instances[instance], addr, dst, len, nostop, timeout_us);
  
  // Try using the SDK function
  // we just drain abort after? :)
  ret = i2c_read_timeout_us(_i2c_instances[instance], addr, dst, len, nostop, timeout_us);
  _i2c_hal_drain_abort(_i2c_instances[instance]->hw);

  if(baud_khz_override)
  {
    i2c_set_baudrate(_i2c_instances[instance], baud_original);
  }
  _bus_exit(instance, nostop);

  return ret;
}

int i2c_hal_read_timeout_us(uint8_t instance, uint8_t addr, uint8_t *dst, size_t len, bool nostop, int timeout_us)
{
  return i2c_hal_read_timeout_us_odbaud(instance, addr, dst, len, nostop, timeout_us, 0);
}

int i2c_hal_write_blocking(uint8_t instance, uint8_t addr, const uint8_t *src, size_t len, bool nostop)
{
  if (instance >= I2C_HAL_MAX_INSTANCES)
    return -1;
  int ret = 0;

  _bus_enter(instance);
  ret = i2c_write_blocking(_i2c_instances[instance], addr, src, len, nostop);
  _bus_exit(instance, nostop);

  return ret;
}

// Write then read in sequence (For reading a specific register, as an example)
int i2c_hal_write_read_timeout_us(uint8_t instance, uint8_t addr, const uint8_t *src,
                                  size_t wr_len, uint8_t *dst, size_t dst_len, int timeout_us)
{
  if (instance >= I2C_HAL_MAX_INSTANCES)
    return -1;
  int ret = 0;

  _bus_enter(instance);
  ret = i2c_write_timeout_us(_i2c_instances[instance], addr, src, wr_len, true, timeout_us);
  ret = i2c_hal_read_timeout_us(instance, addr, dst, dst_len, false, timeout_us);
  _bus_exit(instance, false);

  return ret;
}

// Write then read in sequence (For reading a specific register, as an example)
int i2c_hal_write_read_blocking(uint8_t instance, uint8_t addr, const uint8_t *src,
                                size_t wr_len, uint8_t *dst, size_t dst_len)
{
  if (instance >= I2C_HAL_MAX_INSTANCES)
    return -1;
  int ret = 0;

  _bus_enter(instance);
  ret = i2c_write_blocking(_i2c_instances[instance], addr, src, wr_len, true);
  ret = i2c_read_blocking(_i2c_instances[instance], addr, dst, dst_len, false);
  _i2c_hal_drain_abort(_i2c_instances[instance]->hw);
  _bus_exit(instance, false);

  return ret;
}
