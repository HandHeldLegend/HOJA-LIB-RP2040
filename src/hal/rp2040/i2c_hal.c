#include "hal/i2c_hal.h"
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "pico/timeout_helper.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

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

// Queues a whole register read: the register byte, then a read command per byte, the last with a
// STOP. The FIFO holds it all, so the bus never waits on the CPU between bytes.
static void __not_in_flash_func(_queue_reg_read)(i2c_hw_t *hw, uint8_t addr, uint8_t reg, uint8_t len)
{
  hw->enable = 0;
  hw->tar = addr;
  hw->enable = 1;

  hw->data_cmd = reg;
  for (uint8_t i = 0; i < len; i++)
  {
    hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS |
                   ((i == 0) ? I2C_IC_DATA_CMD_RESTART_BITS : 0) |
                   ((i == len - 1) ? I2C_IC_DATA_CMD_STOP_BITS : 0);
  }
}

// Background register bursts (see i2c_hal_read_reg_burst_async)
typedef struct
{
  i2c_hal_burst_s bursts[I2C_HAL_ASYNC_MAX];
  uint8_t count;
  uint8_t index;
  volatile uint8_t done_mask;
  volatile bool busy;
  bool started;
  bool irq_ready;
} i2c_hal_async_s;
static i2c_hal_async_s _async[I2C_HAL_MAX_INSTANCES] = {0};

static void __not_in_flash_func(_async_start_burst)(uint8_t instance)
{
  const i2c_hal_burst_s *b = &_async[instance].bursts[_async[instance].index];
  _queue_reg_read(_i2c_instances[instance]->hw, b->addr, b->reg, b->len);
}

static void __not_in_flash_func(_async_irq)(uint8_t instance)
{
  i2c_hal_async_s *a = &_async[instance];
  i2c_hw_t *hw = _i2c_instances[instance]->hw;
  const uint32_t stat = hw->raw_intr_stat;

  if (!a->busy)
  {
    hw->intr_mask = 0;
    return;
  }
  if (!(stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS))
    return;

  // The STOP ends every burst, an aborted one too (the hardware flushes the FIFOs then)
  const bool aborted = (stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS) != 0;
  (void) hw->clr_tx_abrt;
  (void) hw->clr_stop_det;

  const i2c_hal_burst_s *b = &a->bursts[a->index];
  uint8_t got = 0;
  while (hw->rxflr && got < b->len)
    b->dst[got++] = (uint8_t)hw->data_cmd;
  while (hw->rxflr)
    (void) hw->data_cmd;
  if (!aborted && got == b->len)
    a->done_mask |= (uint8_t)(1u << a->index);

  if (++a->index < a->count)
  {
    _async_start_burst(instance);
    return;
  }

  hw->intr_mask = 0;
  a->busy = false;
}

static void __not_in_flash_func(_async_irq_0)(void) { _async_irq(0); }
static void __not_in_flash_func(_async_irq_1)(void) { _async_irq(1); }

static void _async_finish(uint8_t instance, int timeout_us)
{
  i2c_hal_async_s *a = &_async[instance];
  if (!a->busy) return;

  const absolute_time_t until = make_timeout_time_us(timeout_us);
  while (a->busy)
  {
    if (time_reached(until))
    {
      // Stuck: give the bus back to the blocking calls
      _i2c_instances[instance]->hw->intr_mask = 0;
      a->busy = false;
      break;
    }
    tight_loop_contents();
  }
}

bool i2c_hal_read_reg_burst_async(uint8_t instance, const i2c_hal_burst_s *bursts, uint8_t count)
{
  if (instance >= I2C_HAL_MAX_INSTANCES || !bursts || count == 0 || count > I2C_HAL_ASYNC_MAX)
    return false;
  if (_bus_hooks[instance])
    return false;
  for (uint8_t i = 0; i < count; i++)
    if (!bursts[i].dst || bursts[i].len == 0 || bursts[i].len > I2C_HAL_BURST_MAX)
      return false;

  i2c_hal_async_s *a = &_async[instance];
  _async_finish(instance, 2000);

  i2c_inst_t *i2c = _i2c_instances[instance];
  i2c_hw_t *hw = i2c->hw;

  if (!a->irq_ready)
  {
    // Every source is unmasked out of reset: only STOP is wanted, and only while bursts run
    hw->intr_mask = 0;
    const uint irq = (instance == 0) ? I2C0_IRQ : I2C1_IRQ;
    irq_set_exclusive_handler(irq, (instance == 0) ? _async_irq_0 : _async_irq_1);
    irq_set_enabled(irq, true);
    a->irq_ready = true;
  }

  for (uint8_t i = 0; i < count; i++)
    a->bursts[i] = bursts[i];
  a->count = count;
  a->index = 0;
  a->done_mask = 0;
  a->started = true;
  a->busy = true;

  (void) hw->clr_tx_abrt;
  (void) hw->clr_stop_det;
  i2c->restart_on_next = false;
  hw->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS;
  _async_start_burst(instance);
  return true;
}

uint8_t i2c_hal_async_wait(uint8_t instance, int timeout_us)
{
  if (instance >= I2C_HAL_MAX_INSTANCES)
    return 0;
  i2c_hal_async_s *a = &_async[instance];
  if (!a->started)
    return 0;
  _async_finish(instance, timeout_us);
  a->started = false;
  return a->done_mask;
}

static void _bus_enter(uint8_t instance)
{
  // Background bursts own the bus until they finish
  _async_finish(instance, 2000);
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

int i2c_hal_read_reg_burst_us(uint8_t instance, uint8_t addr, uint8_t reg, uint8_t *dst, size_t len, int timeout_us)
{
  if (instance >= I2C_HAL_MAX_INSTANCES || !dst || len == 0 || len > I2C_HAL_BURST_MAX)
    return -1;

  i2c_inst_t *i2c = _i2c_instances[instance];
  i2c_hw_t *hw = i2c->hw;
  int ret = (int)len;

  _bus_enter(instance);
  _queue_reg_read(hw, addr, reg, (uint8_t)len);

  const absolute_time_t until = make_timeout_time_us(timeout_us);
  size_t got = 0;
  while (got < len)
  {
    if (hw->rxflr)
    {
      dst[got++] = (uint8_t)hw->data_cmd;
      continue;
    }
    if (hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS)
    {
      ret = PICO_ERROR_GENERIC;
      break;
    }
    if (time_reached(until))
    {
      ret = PICO_ERROR_TIMEOUT;
      break;
    }
  }

  // The STOP (sent by the hardware on an abort too) must finish before the next transfer
  while (!(hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS) && !time_reached(until))
    tight_loop_contents();
  _i2c_hal_drain_abort(hw);
  (void) hw->clr_stop_det;
  i2c->restart_on_next = false;

  _bus_exit(instance, false);
  return ret;
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
