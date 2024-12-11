#include "hoja.h"

#include "usb/usb.h"
#include "wired/wired.h"

#include "hoja_system.h"
#include "hal/hal.h"
#include "hal/sys_hal.h"
#include "drivers/drivers.h"
#include "board_config.h"

#include "utilities/callback.h"
#include "utilities/settings.h"

#include "input/input.h"

#include "devices/battery.h"

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
  static bool deinit = false;
  if(deinit) return;
  deinit = true;

  //cb_hoja_set_uart_enabled(false);
  //cb_hoja_set_bluetooth_enabled(false);

  //sleep_ms(100);

  #if (HOJA_CAPABILITY_RGB == 1 && HOJA_CAPABILITY_BATTERY == 1)
  //rgb_shutdown_start(false, cb);
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
  battery_set_ship_mode();
#else
  hoja_shutdown_instant();
#endif
}

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  static uint32_t c0_timestamp = 0;

  c0_timestamp = sys_hal_time_us();

  input_digital_task(c0_timestamp);

  // Handle HD rumble
  //#if defined(HOJA_CONFIG_HDRUMBLE) && (HOJA_CONFIG_HDRUMBLE==1)
  //  hdrumble_hal_task(c0_timestamp);
  //#endif

  // Comms task
  if(_hoja_mode_task_cb!=NULL)
  {
    _hoja_mode_task_cb(c0_timestamp);
  }

  sys_hal_tick();
}

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  static uint32_t c1_timestamp = 0;

  for (;;)
  {
    c1_timestamp = sys_hal_time_us();

    input_analog_task(c1_timestamp);
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

gamepad_mode_t hoja_gamepad_mode_get()
{
  if(_hoja_current_gamepad_mode != GAMEPAD_MODE_UNDEFINED)
  return _hoja_current_gamepad_mode;

  return GAMEPAD_MODE_SWPRO;
}

void hoja_init()
{
  hal_init();
  input_init();
  _gamepad_mode_init(GAMEPAD_MODE_SWPRO, GAMEPAD_METHOD_USB);

  settings_init();

  // Init specific GAMEPAD mode
  sys_hal_start_dualcore(_hoja_task_0, _hoja_task_1);
}
