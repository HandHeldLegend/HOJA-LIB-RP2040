#include "hoja.h"

button_data_s _button_data = {0};
button_data_s _button_data_processed = {0};

// The raw input analog data
a_data_s _analog_data_input = {0};
// The buffered analog data
// This is processed, but not the final output
a_data_s _analog_data_buffered = {0};
// This is the outgoing analog data
a_data_s _analog_data_output = {0};

button_remap_s *_hoja_remap = NULL;
input_method_t _hoja_input_method = INPUT_METHOD_AUTO;

uint32_t _timer_owner_0;
uint32_t _timer_owner_1;
auto_init_mutex(_hoja_timer_mutex);

volatile uint32_t _hoja_timestamp = 0;

__attribute__((weak)) uint16_t cb_hoja_hardware_test()
{
  return 0;
}

// USER DEFINED CALLBACKS
// DO NOT EDIT
__attribute__((weak)) void cb_hoja_hardware_setup()
{
}

__attribute__((weak)) void cb_hoja_read_buttons(button_data_s *data)
{
  (void)&data;
}

__attribute__((weak)) void cb_hoja_read_analog(a_data_s *data)
{
  (void)&data;
}

__attribute__((weak)) void cb_hoja_read_imu(imu_data_s *data_a, imu_data_s *data_b)
{
  (void)&data_a;
  (void)&data_b;
}

__attribute__((weak)) void cb_hoja_rumble_enable(float intensity)
{
  (void)intensity;
}

__attribute__((weak)) void cb_hoja_task_1_hook(uint32_t timestamp)
{
  (void)timestamp;
}

__attribute__((weak)) void cb_hoja_set_rumble_intensity(uint8_t floor, uint8_t intensity)
{
  (void)intensity;
}

__attribute__((weak)) void cb_hoja_set_bluetooth_enabled(bool enable)
{
  (void)enable;
}

__attribute__((weak)) void cb_hoja_set_uart_enabled(bool enable)
{
  (void)enable;
}

input_method_t hoja_get_input_method()
{
  return _hoja_input_method;
}

a_data_s *hoja_get_buffered_analog_data()
{
  return &_analog_data_buffered;
}

button_data_s *hoja_get_raw_button_data()
{
  return &_button_data;;
}

void hoja_shutdown_instant()
{
    cb_hoja_set_bluetooth_enabled(false);
    cb_hoja_set_uart_enabled(false);
    watchdog_reboot(0, 0, 0);
}

void hoja_shutdown()
{
  static bool _shutdown_started = false;
  if(_shutdown_started) return;
  
  _shutdown_started = true;
  #if (HOJA_CAPABILITY_RGB == 1 && HOJA_CAPABILITY_BATTERY == 1)
    rgb_shutdown_start(false);
  #elif (HOJA_CAPABILITY_BATTERY == 1)
    util_battery_enable_ship_mode();
  #else 
    hoja_shutdown_instant();
  #endif
}

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  if (mutex_try_enter(&_hoja_timer_mutex, &_timer_owner_0))
  {
    _hoja_timestamp = time_us_32();
    mutex_exit(&_hoja_timer_mutex);
  }

  // Read buttons
  cb_hoja_read_buttons(&_button_data);
  macro_handler_task(_hoja_timestamp, &_button_data);
  remap_buttons_task();
  rgb_task(_hoja_timestamp);

  // Webusb stuff
  if (webusb_output_enabled())
  {
    snapback_webcapture_task(_hoja_timestamp, &_analog_data_buffered);
    webusb_input_report_task(_hoja_timestamp, &_analog_data_buffered, NULL);
  }


  // Our communication core task
  hoja_comms_task(_hoja_timestamp, &_button_data_processed, &_analog_data_output);
  

  if (_hoja_input_method == INPUT_METHOD_USB)
  {
    util_battery_monitor_task_usb(_hoja_timestamp);
  }

}

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  for (;;)
  {

    if (mutex_try_enter(&_hoja_timer_mutex, &_timer_owner_1))
    {
      _hoja_timestamp = time_us_32();
      mutex_exit(&_hoja_timer_mutex);
    }

    // Check if we need to save
    settings_core1_save_check();

    // Do analog stuff :)
    analog_task(_hoja_timestamp);

    // Do IMU stuff
    imu_task(_hoja_timestamp);

    // Do callback for userland code insertion
    cb_hoja_task_1_hook(_hoja_timestamp);
  }
}

void hoja_init(hoja_config_t *config)
{
  // Stop if there's no config
  if (!config)
    return;

  // Get our reboot reason
  hoja_reboot_memory_u reboot_mem = {.value = reboot_get_memory()};

  if(reboot_mem.reboot_reason == ADAPTER_REBOOT_REASON_BTSTART)
  {
    // We're rebooting from a BT start
    // We need to set the input method to BT
    config->input_method = INPUT_METHOD_BLUETOOTH;
  }

  // Set up hardware first
  cb_hoja_hardware_setup();

#if ( (HOJA_CAPABILITY_BLUETOOTH) == 1 || (HOJA_CAPABILITY_BATTERY == 1) )
  // I2C Setup
  i2c_init(HOJA_I2C_BUS, 200 * 1000);
  gpio_set_function(HOJA_I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(HOJA_I2C_SCL, GPIO_FUNC_I2C);
#endif

  // Read buttons to get a current state
  cb_hoja_read_buttons(&_button_data);

  // Load settings and related logic
  {
    bool settings_loaded = settings_load();

    if (!settings_loaded)
    {
      settings_reset_to_default();
      sleep_ms(200);
    
      analog_init(&_analog_data_input, &_analog_data_output, &_analog_data_buffered, &_button_data);
    }
    else
    {
      analog_init(&_analog_data_input, &_analog_data_output, &_analog_data_buffered, &_button_data);
    }
  }

  // Reset pairing if needed.
  if(_button_data.button_sync)
  {
    memset(global_loaded_settings.switch_host_address, 0, 6);
  }

  // Set rumble intensity
  cb_hoja_set_rumble_intensity(global_loaded_settings.rumble_floor, global_loaded_settings.rumble_intensity);

  // For switch Pro stuff
  switch_analog_calibration_init();

  input_mode_t _hoja_input_mode = 0;

  if (config->input_mode == INPUT_MODE_LOAD)
  {
    _hoja_input_mode = global_loaded_settings.input_mode;
  }
  else
  {
    _hoja_input_mode = config->input_mode;
  }

  if(config->input_method != INPUT_METHOD_AUTO)
  {
    _hoja_input_method = config->input_method;
  }

  // Macros for mode switching
  {
    if(_button_data.button_b)
    {
      _hoja_input_mode = INPUT_MODE_DS4;
    }
    else if (_button_data.button_x)
    {
      _hoja_input_mode = INPUT_MODE_XINPUT;
    }
    else if (_button_data.button_a)
    {
      _hoja_input_mode = INPUT_MODE_SWPRO;
    }
    else if (_button_data.button_y)
    {
      _hoja_input_mode = INPUT_MODE_GCUSB;
    }
    else if (_button_data.dpad_left)
    {
      _hoja_input_mode = INPUT_MODE_SNES;
    }
    else if (_button_data.dpad_down && !_button_data.dpad_right)
    {
      _hoja_input_mode = INPUT_MODE_N64;
    }
    else if (_button_data.dpad_right)
    {
      _hoja_input_mode = INPUT_MODE_GAMECUBE;
    }
  }

  rgb_mode_t rgbmode = global_loaded_settings.rgb_mode;
  uint8_t rgbbrightness = 100;
  uint32_t indicate_color = COLOR_WHITE.color;

  // Checks for retro and modes where we don't care about
  // checking the plug status
  switch (_hoja_input_mode)
  {
    case INPUT_MODE_GCUSB:
      indicate_color = COLOR_CYAN.color;
      _hoja_input_method = INPUT_METHOD_USB;
      break;
    case INPUT_MODE_XINPUT:
    case INPUT_MODE_XHID:
      indicate_color = COLOR_GREEN.color;
      break;

    default:
    case INPUT_MODE_SWPRO:
      indicate_color = COLOR_WHITE.color;
      break;

    case INPUT_MODE_DS4:
      indicate_color = COLOR_BLUE.color;
      break;

    case INPUT_MODE_SNES:
    case INPUT_MODE_GAMECUBE:
    case INPUT_MODE_N64:
      rgbbrightness = 15;
      indicate_color = COLOR_PURPLE.color;
      break;
  }

  if (config->input_method == INPUT_METHOD_AUTO)
  {
    if (!util_wire_connected())
    {
      
      rgbbrightness = 70;
      _hoja_input_method = INPUT_METHOD_BLUETOOTH;
    }
    else
    {
      util_battery_set_charge_rate(100);
      _hoja_input_method = INPUT_METHOD_USB;
    }
  }

  rgb_init(rgbmode, rgbbrightness);
  //rgb_init(RGB_MODE_REACTIVE, rgbbrightness);
  rgb_indicate(indicate_color);

  hoja_comms_init(_hoja_input_mode, _hoja_input_method);

  if (_button_data.button_home)
  {
    global_loaded_settings.input_mode = _hoja_input_mode;
    settings_save();
  }

  // Initialize button remapping
  remap_init(_hoja_input_mode, &_button_data, &_button_data_processed);

  // Enable lockout victimhood :,)
  multicore_lockout_victim_init();

  // Launch second core
  multicore_launch_core1(_hoja_task_1);

  for (;;)
  {
    _hoja_task_0();
  }
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
  // printf("Set up GPIO: %d", (uint8_t) gpio);
}

void hoja_setup_gpio_button(uint8_t gpio)
{
  gpio_init(gpio);
  gpio_pull_up(gpio);
  gpio_set_dir(gpio, GPIO_IN);
  // printf("Set up GPIO: %d", (uint8_t) gpio);
}
