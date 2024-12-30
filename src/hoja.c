#include "hoja.h"

#include "usb/usb.h"
#include "usb/webusb.h"
#include "wired/wired.h"

#include "hoja_system.h"

#include "hal/sys_hal.h"
#include "hal/flash_hal.h"
#include "hal/i2c_hal.h"
#include "hal/spi_hal.h"

#include "board_config.h"

#include "utilities/callback.h"
#include "utilities/settings.h"
#include "utilities/boot.h"

#include "input/button.h"
#include "input/analog.h"
#include "input/imu.h"
#include "input/triggers.h"
#include "input/macros.h"

#include "devices/battery.h"
#include "devices/rgb.h"

time_callback_t   _hoja_mode_task_cb = NULL;
gamepad_mode_t    _hoja_current_gamepad_mode    = GAMEPAD_MODE_LOAD;
gamepad_method_t  _hoja_current_gamepad_method  = GAMEPAD_METHOD_AUTO;

__attribute__((weak)) bool cb_hoja_buttons_init()
{
  return false;
}

__attribute__((weak)) void cb_hoja_read_buttons(button_data_s *data)
{
  (void)&data;
}

void hoja_deinit(callback_t cb)
{
  static bool deinit_lockout = false;
  if(deinit_lockout) return;
  deinit_lockout = true;

  //cb_hoja_set_uart_enabled(false);
  //cb_hoja_set_bluetooth_enabled(false);

  //sleep_ms(100);

  #if defined(HOJA_RGB_DRIVER)
  rgb_deinit(cb);
  #else
  cb();
  #endif
}

void hoja_shutdown() 
{
  battery_set_ship_mode();
}

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  static uint32_t c0_timestamp = 0;

  // Get current system timestamp
  c0_timestamp = sys_hal_time_us();

  // Read/process buttons/analog triggers
  button_task(c0_timestamp);

  // Process any macros
  macros_task(c0_timestamp);

  // Handle haptics
  haptics_task(c0_timestamp);

  // Comms task
  if(_hoja_mode_task_cb!=NULL)
  {
    _hoja_mode_task_cb(c0_timestamp);

    // Optional web Input
    webusb_send_rawinput(c0_timestamp);
  }

  // RGB task
  rgb_task(c0_timestamp);

  // Battery task
  battery_task(c0_timestamp);

  // Update sys tick
  sys_hal_tick();
}

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  static uint32_t c1_timestamp = 0;

  // Init settings hal
  //flash_hal_init();

  for (;;)
  {
    c1_timestamp = sys_hal_time_us();

    // Analog task
    analog_task(c1_timestamp);

    // IMU task
    imu_task(c1_timestamp);

    // Flash task
    flash_hal_task();
  }
}


// Replace with proper boot function later TODO
bool _gamepad_mode_init(gamepad_mode_t mode, gamepad_method_t method)
{
  bool ret = false;
  switch(mode)
  {
    default:
    case GAMEPAD_MODE_SWPRO:
      _hoja_current_gamepad_method = GAMEPAD_METHOD_USB;
      _hoja_current_gamepad_mode   = mode;
      _hoja_mode_task_cb = usb_mode_task;
      usb_mode_start(GAMEPAD_MODE_SWPRO);
    break;
  }

  return true;
}

bool _system_requirements_init()
{
  // System hal init
  sys_hal_init();

  // SPI 0
  #if defined(HOJA_SPI_0_ENABLE) && (HOJA_SPI_0_ENABLE==1)
  spi_hal_init(0, HOJA_SPI_0_GPIO_CLK, HOJA_SPI_0_GPIO_MISO, HOJA_SPI_0_GPIO_MOSI);
  #endif

  // I2C 0
  #if defined(HOJA_I2C_0_ENABLE) && (HOJA_I2C_0_ENABLE==1)
  i2c_hal_init(0, HOJA_I2C_0_GPIO_SDA, HOJA_I2C_0_GPIO_SCL);
  #endif

  // System settings
  settings_init();

  return true;
}

bool _system_devices_init()
{
  // Battery 
  battery_init();

  // Haptics
  haptics_init();

  // RGB
  rgb_init(-1, -1);
  return true;
}

bool _system_input_init()
{
  // Buttons
  button_init();

  // Analog joysticks
  analog_init();

  // Analog triggers
  triggers_init();

  // IMU motion
  imu_init();
  return true;
}

gamepad_mode_t hoja_gamepad_mode_get()
{
  if(_hoja_current_gamepad_mode != GAMEPAD_MODE_UNDEFINED)
  return _hoja_current_gamepad_mode;

  return GAMEPAD_MODE_SWPRO;
}

void hoja_init()
{
  _system_requirements_init();
  _system_input_init();
  _system_devices_init();

  gamepad_mode_t   mode   = GAMEPAD_MODE_SWPRO;
  gamepad_method_t method = GAMEPAD_METHOD_USB;

  // Check input mode selection based on button combos
  gamepad_mode_t boot_mode = boot_get_mode_selection();

  boot_memory_s boot_memory = {0};
  boot_get_memory(&boot_memory);

  // If we have reboot memory, use it
  if(boot_memory.val)
  {
    mode    = boot_memory.gamepad_mode;
    method  = boot_memory.gamepad_protocol;
  }
  // If we have no boot mem, and we have
  // a selected boot mode, use it
  else if(boot_mode != GAMEPAD_MODE_UNDEFINED)
  {
    mode = boot_mode;
  }

  // DEBUG always USB for now
  method = GAMEPAD_METHOD_USB;

  // Init gamepad mode with the method
  _gamepad_mode_init(mode, method);

  // Init tasks finally
  sys_hal_start_dualcore(_hoja_task_0, _hoja_task_1);
}
