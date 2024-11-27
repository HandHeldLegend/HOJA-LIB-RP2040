#include "hoja.h"
#include "hoja_system.h"
#include "hal/hal.h"
#include "hal/sys_hal.h"
#include "drivers/drivers.h"
#include "board_config.h"

#include "devices/devices.h"
#include "devices/haptics.h"

#include "input/button.h"


#define CENTER 2048

device_method_t _hoja_input_method = DEVICE_METHOD_AUTO;

uint32_t _timer_owner_0;
auto_init_mutex(_hoja_timer_mutex);


__attribute__((weak)) bool cb_hoja_buttons_init()
{
  return false;
}

__attribute__((weak)) void cb_hoja_read_buttons(button_data_s *data)
{
  (void)&data;
}

void _hoja_hal_setup()
{
  sys_hal_init();

  // SPI 0
  #if defined(HOJA_SPI_0_ENABLE) && (HOJA_SPI_0_ENABLE==1)
  spi_hal_init(0, HOJA_SPI_0_GPIO_CLK, HOJA_SPI_0_GPIO_MISO, HOJA_SPI_0_GPIO_MOSI);
  #endif

  // I2C 0
  #if defined(HOJA_I2C_0_ENABLE) && (HOJA_I2C_0_ENABLE==1)
  i2c_hal_init(0, HOJA_I2C_0_GPIO_SDA, HOJA_I2C_0_GPIO_SCL);
  #endif

  // HD Rumble
  #if defined(HOJA_CONFIG_HDRUMBLE) && (HOJA_CONFIG_HDRUMBLE==1)
  hdrumble_hal_init();
  #endif
}

void hoja_deinit(callback_t cb)
{
  static bool deinit = false;
  if(deinit) return;
  deinit = true;

  cb_hoja_set_uart_enabled(false);
  cb_hoja_set_bluetooth_enabled(false);

  sleep_ms(100);

  #if (HOJA_CAPABILITY_RGB == 1 && HOJA_CAPABILITY_BATTERY == 1)
  rgb_shutdown_start(false, cb);
  #else
  cb();
  #endif
}

void hoja_shutdown()
{
  static bool _shutdown_started = false;
  if (_shutdown_started)
    return;

  _shutdown_started = true;

#if (HOJA_CAPABILITY_BATTERY == 1)
  battery_enable_ship_mode();
#else
  hoja_shutdown_instant();
#endif
}

bool _watchdog_enabled = false;

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  static uint32_t c0_timestamp = 0;

  c0_timestamp = sys_hal_time_us();

  rgb_task(c0_timestamp);

  // Handle HD rumble
  #if defined(HOJA_CONFIG_HDRUMBLE) && (HOJA_CONFIG_HDRUMBLE==1)
    hdrumble_hal_task(c0_timestamp);
  #endif

  sys_hal_tick();
}



// Core 1 task loop entrypoint
void _hoja_task_1()
{
  static uint32_t c1_timestamp = 0;

  for (;;)
  {
    c1_timestamp = sys_hal_time_us();

    // Check if we need to save
    settings_core1_save_check();

    // Do analog stuff :)
    analog_task(c1_timestamp);

    // Do IMU stuff
    imu_task(c1_timestamp);
  }
}

void hoja_init()
{
  if(!cb_hoja_buttons_init())
  {
    // reset to USB bootloader if we didn't handle this.
  }

  _hoja_hal_setup();
  _hoja_driver_setup();

  sys_hal_start(_hoja_task_0, _hoja_task_1);
}

void hoja_setup_gpio_scan(uint8_t gpio)
{
  gpio_init(gpio);
  gpio_pull_up(gpio);
  gpio_set_dir(gpio, GPIO_OUT);
  gpio_put(gpio, true);
}

void hoja_setup_gpio_push(uint8_t gpio)
{
  gpio_init(gpio);
  gpio_pull_up(gpio);
  gpio_set_dir(gpio, GPIO_IN);
}

void hoja_setup_gpio_button(uint8_t gpio)
{
  gpio_init(gpio);
  gpio_pull_up(gpio);
  gpio_set_dir(gpio, GPIO_IN);
}
