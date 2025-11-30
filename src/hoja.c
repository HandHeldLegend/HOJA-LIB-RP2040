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
#include "utilities/sysmon.h"

#include "input/analog.h"
#include "input/imu.h"
#include "input/macros.h"
#include "input/idle_manager.h"
#include "input/mapper.h"
#include "input/hover.h"

#include "devices/battery.h"
#include "devices/rgb.h"
#include "devices/bluetooth.h"
#include "wired/wired.h"
#include "devices/haptics.h"
#include "devices/fuelgauge.h"

time_callback_t _hoja_mode_task_cb = NULL;
callback_t _hoja_mode_stop_cb = NULL;

__attribute__((weak)) void cb_hoja_init()
{

}

__attribute__((weak)) void cb_hoja_read_joystick(uint16_t *input)
{
  (void)&input;
}

__attribute__((weak)) void cb_hoja_read_input(mapper_input_s *input)
{
  (void)&input;
}

void hoja_deinit(callback_t cb)
{
  static bool deinit_lockout = false;

  if (deinit_lockout)
    return;
  deinit_lockout = true;

  // Stop our current loop function
  _hoja_mode_task_cb = NULL;

  // Stop current mode if we have a functions
  if (_hoja_mode_stop_cb)
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

  for (;;)
  {
    sys_hal_sleep_ms(1000);
    sys_hal_reboot();
  }
}

void hoja_restart()
{
  // Stop our current loop function
  _hoja_mode_task_cb = NULL;

  // Stop current mode if we have a functions
  if (_hoja_mode_stop_cb)
    _hoja_mode_stop_cb();

  sys_hal_reboot();
}

rgb_s _gamepad_mode_color_get(gamepad_mode_t mode)
{
  switch (mode)
  {
  case GAMEPAD_MODE_SWPRO:
    return COLOR_WHITE;
    break;

  case GAMEPAD_MODE_SINPUT:
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

volatile hoja_status_s _hoja_status = {
    .connection_status = CONN_STATUS_INIT,
    .notification_color = 0,
    .gamepad_color = 0,
    .gamepad_method = 0,
    .gamepad_mode = 0,
    .init_status = false};

void hoja_set_connected_status(connection_status_t status)
{
  _hoja_status.connection_status = status;
}

void hoja_set_notification_status(rgb_s color)
{
  _hoja_status.notification_color = color;
}

void hoja_clr_ss_notif()
{
  _hoja_status.ss_notif_pending = false;
}

void hoja_set_ss_notif(rgb_s color)
{
  _hoja_status.ss_notif_pending = true;
  _hoja_status.ss_notif_color = color;
}

void hoja_set_debug_data(uint8_t data)
{
  _hoja_status.debug_data = data;
}

hoja_status_s hoja_get_status()
{
  return _hoja_status;
}

gamepad_mode_t thisMode = GAMEPAD_MODE_SWPRO;
gamepad_method_t thisMethod = GAMEPAD_METHOD_AUTO;
bool thisPair = false;

// Replace with proper boot function later TODO
bool _gamepad_mode_init(gamepad_mode_t mode, gamepad_method_t method, bool pair)
{
  if (method == GAMEPAD_METHOD_AUTO)
  {
    battery_status_s status;
    battery_get_status(&status);

    // PMIC unused
    if (!status.connected)
    {
      method = GAMEPAD_METHOD_USB;
    }
    else
    {
      if (!status.plugged)
      {
        switch (mode)
        {
        case GAMEPAD_MODE_SWPRO:
        case GAMEPAD_MODE_SINPUT:
        case GAMEPAD_MODE_XINPUT:
          method = GAMEPAD_METHOD_BLUETOOTH;
          break;

        default:
          method = GAMEPAD_METHOD_USB;
          break;
        }
      }
      else
      {
        method = GAMEPAD_METHOD_USB;
      }
    }
  }

  // debug
  // method = GAMEPAD_METHOD_BLUETOOTH;

  _hoja_status.gamepad_mode = mode;
  _hoja_status.gamepad_method = method;
  _hoja_status.gamepad_color = _gamepad_mode_color_get(mode);

  hoja_set_connected_status(CONN_STATUS_CONNECTING); // Pending
  hoja_set_ss_notif(_hoja_status.gamepad_color);

  switch (method)
  {
  default:
  case GAMEPAD_METHOD_USB:
    battery_set_charge_rate(225);
    _hoja_mode_task_cb = usb_mode_task;
    usb_mode_start(mode);
    break;

  case GAMEPAD_METHOD_WIRED:
    battery_set_charge_rate(0);
    _hoja_mode_task_cb = wired_mode_task;
    wired_mode_start(mode);
    break;

  case GAMEPAD_METHOD_BLUETOOTH:
    battery_set_charge_rate(250);
    _hoja_mode_task_cb = bluetooth_mode_task;
    _hoja_mode_stop_cb = bluetooth_mode_stop;
    bluetooth_mode_start(mode, pair);
    break;
  }

  // Reload our remap
  mapper_init();
  return true;
}

#include "hardware/sync.h"

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  static uint64_t c1_timestamp = 0;

  // init gamepad mode on core 1
  _gamepad_mode_init(thisMode, thisMethod, thisPair);

  for (;;)
  {
    // Get current system timestamp
    sys_hal_time_us(&c1_timestamp);

    // Flash task
    flash_hal_task();

    // Process any macros
    macros_task(c1_timestamp);

    if (webusb_outputting_check())
    {
      // Optional web Input
      webusb_send_rawinput(c1_timestamp);
    }
    else if (_hoja_mode_task_cb)
    {
      _hoja_mode_task_cb(c1_timestamp);
    }

    // Handle haptics
    haptics_task(c1_timestamp);

    // RGB task
    rgb_task(c1_timestamp);

    // System Monitor task (battery/fuel gauge)
    sysmon_task(c1_timestamp);

    // Update sys tick
    sys_hal_tick();

    // Idle manager
    idle_manager_task(c1_timestamp);

    // IMU task
    imu_task(c1_timestamp);
  }
}

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  static uint64_t c0_timestamp = 0;

  // We can lock core 0 for writing to flash
  flash_hal_init();

  for (;;)
  {
    sys_hal_time_us(&c0_timestamp);

    // Analog task
    analog_task(c0_timestamp);
  }
}

bool _system_requirements_init()
{
  // System hal init
  sys_hal_init();

// SPI 0
#if defined(HOJA_SPI_0_ENABLE) && (HOJA_SPI_0_ENABLE == 1)
  spi_hal_init(0, HOJA_SPI_0_GPIO_CLK, HOJA_SPI_0_GPIO_MISO, HOJA_SPI_0_GPIO_MOSI);
#endif

// I2C 0
#if defined(HOJA_I2C_0_ENABLE) && (HOJA_I2C_0_ENABLE == 1)
  uint32_t baudrate_khz_i2c0 = 400; // Default baudrate
#if defined(HOJA_I2C_0_BAUDRATE_KHZ)
  baudrate_khz_i2c0 = HOJA_I2C_0_BAUDRATE_KHZ;
#endif
  i2c_hal_init(0, HOJA_I2C_0_GPIO_SDA, HOJA_I2C_0_GPIO_SCL, baudrate_khz_i2c0);
#endif

// I2C 1
#if defined(HOJA_I2C_1_ENABLE) && (HOJA_I2C_1_ENABLE == 1)
  uint32_t baudrate_khz_i2c1 = 400; // Default baudrate
#if defined(HOJA_I2C_1_BAUDRATE_KHZ)
  baudrate_khz_i2c1 = HOJA_I2C_1_BAUDRATE_KHZ;
#endif
  i2c_hal_init(1, HOJA_I2C_1_GPIO_SDA, HOJA_I2C_1_GPIO_SCL, baudrate_khz_i2c1);
#endif

  // System settings
  settings_init();

  // MAIN.C BOARD INIT
  cb_hoja_init();

  return true;
}

bool _system_devices_init(gamepad_method_t method, gamepad_mode_t mode)
{
  // Battery
  if(method!=GAMEPAD_METHOD_WIRED)
    battery_init();

  // Fuel gauge
  fuelgauge_init(1200);

  // Haptics
  haptics_init();

  int bright = -1;

  if (method == GAMEPAD_METHOD_BLUETOOTH || method == GAMEPAD_METHOD_WIRED)
  {
    const int halfbright = (RGB_BRIGHTNESS_MAX / 3);
    bright = rgb_config->rgb_brightness > halfbright ? halfbright : rgb_config->rgb_brightness;
  }

  // RGB
  rgb_init(-1, bright);
  return true;
}

bool _system_input_init()
{
  // Hover input
  hover_init();

  // Analog joysticks
  analog_init();

  // IMU motion
  imu_init();
  return true;
}

void hoja_init()
{
  _system_requirements_init();
  _system_input_init();

  boot_get_mode_method(&thisMode, &thisMethod, &thisPair);

  _system_devices_init(thisMethod, thisMode);

  // Init tasks finally
  sys_hal_start_dualcore(_hoja_task_0, _hoja_task_1);
}
