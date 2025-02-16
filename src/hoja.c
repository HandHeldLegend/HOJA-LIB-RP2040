#include "hoja.h"

#include "usb/usb.h"
#include "usb/webusb.h"
#include "wired/wired.h"

#include "hal/sys_hal.h"
#include "hal/flash_hal.h"
#include "hal/i2c_hal.h"
#include "hal/spi_hal.h"

#include "board_config.h"

#include "utilities/callback.h"
#include "utilities/settings.h"
#include "utilities/boot.h"
#include "utilities/hooks.h"

#include "input/button.h"
#include "input/analog.h"
#include "input/imu.h"
#include "input/trigger.h"
#include "input/macros.h"

#include "devices/battery.h"
#include "devices/rgb.h"
#include "devices/bluetooth.h"
#include "wired/wired.h"
#include "devices/haptics.h"

time_callback_t   _hoja_mode_task_cb = NULL;
callback_t        _hoja_mode_stop_cb = NULL;

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

  // Stop our current loop function
  _hoja_mode_task_cb = NULL;

  // Stop current mode if we have a functions
  if(_hoja_mode_stop_cb)
    _hoja_mode_stop_cb();

  haptics_stop();

  hoja_set_connected_status(CONN_STATUS_SHUTDOWN);
  
  #if defined(HOJA_RGB_DRIVER)
  rgb_deinit(cb);
  #else
  cb();
  #endif
}

void hoja_shutdown() 
{
  // Stop our current loop function
  _hoja_mode_task_cb = NULL;

  battery_set_ship_mode();
  for(;;) {}
}

void hoja_restart()
{
  // Stop our current loop function
  _hoja_mode_task_cb = NULL;

  // Stop current mode if we have a functions
  if(_hoja_mode_stop_cb)
    _hoja_mode_stop_cb();
  
  sys_hal_reboot();
}

rgb_s _gamepad_mode_color_get(gamepad_mode_t mode) 
{
  switch(mode)
  {
      case GAMEPAD_MODE_SWPRO:
        return COLOR_WHITE;
      break;

      case GAMEPAD_MODE_OPENGP:
        return COLOR_BLUE;
      break;

      case GAMEPAD_MODE_XINPUT:
        return COLOR_GREEN;
      break;

      case GAMEPAD_MODE_GAMECUBE:
        return COLOR_PURPLE;
      break;

      case GAMEPAD_MODE_GCUSB:
        return COLOR_CYAN;
      break;

      case GAMEPAD_MODE_N64:
        return COLOR_YELLOW;
      break;

      case GAMEPAD_MODE_SNES:
        return COLOR_RED;
      break;

      default:
        return COLOR_ORANGE;
      break;  
  }
}

volatile hoja_status_s  _hoja_status = {
  .connection_status  = CONN_STATUS_INIT,
  .notification_color = 0,
  .gamepad_color = 0,
  .gamepad_method = 0,
  .gamepad_mode = 0,
  .init_status = false
};

void hoja_set_connected_status(connection_status_t status) 
{
  _hoja_status.connection_status = status;
}

void hoja_set_notification_status(rgb_s color)
{
  _hoja_status.notification_color = color;
}

hoja_status_s hoja_get_status() 
{
  return _hoja_status;
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

  if(webusb_outputting_check())
  {
    // Optional web Input
    webusb_send_rawinput(c0_timestamp);
  }
  else if(_hoja_mode_task_cb)
  {
    _hoja_mode_task_cb(c0_timestamp);
  }

  // RGB task
  rgb_task(c0_timestamp);

  // Battery task
  battery_task(c0_timestamp);

  // Handle haptics
  haptics_task(c0_timestamp);

  // Update sys tick
  sys_hal_tick();

  // Hooks tasks
  //hooks_run_all_core0();
}

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  static uint32_t c1_timestamp = 0;

  // Init settings hal
  //flash_hal_init();

  sys_hal_sleep_ms(500);

  for (;;)
  {
    c1_timestamp = sys_hal_time_us();

    // Analog task
    analog_task(c1_timestamp);

    // Trigger task
    trigger_task(c1_timestamp);

    // IMU task
    imu_task(c1_timestamp);

    // Flash task
    flash_hal_task();

    // Hooks tasks
    //hooks_run_all_core1();
  }
}

// Replace with proper boot function later TODO
bool _gamepad_mode_init()
{

  gamepad_mode_t    thisMode    = GAMEPAD_MODE_SWPRO;
  gamepad_method_t  thisMethod  = GAMEPAD_METHOD_USB;
  bool              thisPair    = false;

  boot_get_mode_method(&thisMode, &thisMethod, &thisPair);

  // Debug
  thisMethod = GAMEPAD_METHOD_BLUETOOTH;

  _hoja_status.gamepad_mode   = thisMode;
  _hoja_status.gamepad_method = thisMethod;
  _hoja_status.gamepad_color = _gamepad_mode_color_get(thisMode);

  hoja_set_connected_status(CONN_STATUS_CONNECTING); // Pending

  switch(thisMethod)
  {
    default:
    case GAMEPAD_METHOD_USB:
      _hoja_mode_task_cb = usb_mode_task;
      usb_mode_start(thisMode);
    break;

    case GAMEPAD_METHOD_WIRED:
      battery_set_plug(BATTERY_PLUG_OVERRIDE);
      _hoja_mode_task_cb = wired_mode_task;
      wired_mode_start(thisMode);
    break;

    case GAMEPAD_METHOD_BLUETOOTH:
      _hoja_mode_task_cb = bluetooth_mode_task;
      _hoja_mode_stop_cb = bluetooth_mode_stop;
      bluetooth_mode_start(thisMode, thisPair);
    break;
  }

  // Reload our remap
  remap_init();
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

  // I2C 1
  #if defined(HOJA_I2C_1_ENABLE) && (HOJA_I2C_1_ENABLE==1)
  i2c_hal_init(1, HOJA_I2C_1_GPIO_SDA, HOJA_I2C_1_GPIO_SCL);
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
  trigger_init();

  // IMU motion
  imu_init();
  return true;
}

void hoja_init()
{
  _system_requirements_init();
  _system_input_init();
  _system_devices_init();

  // Init gamepad mode with the method
  _gamepad_mode_init();

  // Init tasks finally
  sys_hal_start_dualcore(_hoja_task_0, _hoja_task_1);
}
