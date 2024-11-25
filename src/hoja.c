#include "hoja.h"
#include "hoja_system.h"
#include "hoja_hal.h"
#include "drivers/drivers.h"
#include "board_config.h"

#include "devices/devices.h"

#include "input/button.h"
#include "extensions/haptics.h"

#define CENTER 2048

button_data_s _button_data = {0};
button_data_s _button_data_processed = {0};
button_data_s _button_data_output = {0};

// The raw input analog data
analog_data_s _analog_data_input = {0};

// The buffered analog data
// This is the analog data minus snapback for checking in the app
analog_data_s _analog_data_desnapped = {0};

// This is the outgoing analog data
analog_data_s _analog_data_output = {0};

button_remap_s *_hoja_remap = NULL;
device_method_t _hoja_input_method = DEVICE_METHOD_AUTO;

uint32_t _timer_owner_0;
auto_init_mutex(_hoja_timer_mutex);

bool _baseband_loop = false;

__attribute__((weak)) uint16_t cb_hoja_hardware_test()
{
  return 0;
}

__attribute__((weak)) void cb_hoja_read_buttons(button_data_s *data)
{
  (void)&data;
}

void hoja_get_rumble_settings(uint8_t *intensity, rumble_type_t *type)
{
  *intensity = global_loaded_settings.rumble_intensity;
  *type = global_loaded_settings.rumble_mode;
}

__attribute__((weak)) void cb_hoja_set_bluetooth_enabled(bool enable)
{
  (void)enable;
}

__attribute__((weak)) void cb_hoja_set_uart_enabled(bool enable)
{
  (void)enable;
}

__attribute__((weak)) void cb_hoja_baseband_update_loop(button_data_s *buttons)
{
  (void)&buttons;
}

__attribute__((weak)) uint8_t cb_hoja_get_battery_level()
{
  // value between 0 and 100
  return 100;
}

input_method_t hoja_get_input_method()
{
  return _hoja_input_method;
}

analog_data_s *hoja_get_desnapped_analog_data()
{
  return &_analog_data_desnapped;
}

button_data_s *hoja_get_raw_button_data()
{
  return &_button_data;
}

void _hoja_hal_setup()
{
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

bool _hoja_idle_state = false;
#define IDLE_TME_SECONDS 60*5 // 5 minutes
void _hoja_set_idle_state(button_data_s *buttons, analog_data_s *analogs, uint32_t timestamp)
{
  static interval_s interval = {0};
  static interval_s check_interval = {0};
  const uint32_t idle_time = IDLE_TME_SECONDS * 1000 * 1000;
  static uint16_t btns = 0;
  static analog_data_s last_analogs = {0};

  bool reset = false;

  const uint32_t check_time = 1 * 1000 * 1000;
  if (interval_run(timestamp, check_time, &check_interval))
  {

    int lxd = abs(last_analogs.lx - CENTER);
    int lyd = abs(last_analogs.ly - CENTER);
    if (lxd > 100)
      reset = true;
    if (lyd > 100)
      reset = true;

    if (btns > 0)
    {
      reset = true;
      btns = 0;
    }
  }

  btns |= buttons->buttons_all;
  last_analogs.lx = analogs->lx;
  last_analogs.ly = analogs->ly;

  if (interval_resettable_run(timestamp, idle_time, reset, &interval) && (!_hoja_idle_state))
  {
    _hoja_idle_state = true;
  }
  else if (reset && _hoja_idle_state)
  {
    _hoja_idle_state = false;
  }
}

bool hoja_get_idle_state()
{
  return _hoja_idle_state;
}

void hoja_set_baseband_update(bool set)
{
  if (set)
  {
    cb_hoja_set_uart_enabled(true);
    cb_hoja_set_bluetooth_enabled(true);
    _baseband_loop = true;
  }
  else
    _baseband_loop = false;
}

void hoja_shutdown_instant()
{
  cb_hoja_set_bluetooth_enabled(false);
  cb_hoja_set_uart_enabled(false);
  watchdog_reboot(0, 0, 0);
}

uint32_t hoja_get_timestamp()
{
  static uint32_t t = 0;
  if (mutex_try_enter(&_hoja_timer_mutex, &_timer_owner_0))
  {
    t = time_us_32();
    mutex_exit(&_hoja_timer_mutex);
  }
  return t;
}

// This prepares the system for shutdown, then calls a callback function

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

  if (!_watchdog_enabled)
  {
    watchdog_enable(16000, false);
    _watchdog_enabled = true;
  }

  c0_timestamp = hoja_get_timestamp();

  // Read buttons
  cb_hoja_read_buttons(&_button_data);

  if (_baseband_loop)
  {
    cb_hoja_baseband_update_loop(&_button_data);
    watchdog_update();
    rgb_task(c0_timestamp);
    battery_monitor_task(c0_timestamp);
    return;
  }

  remap_buttons_task();
  macro_handler_task(c0_timestamp, &_button_data);

  rgb_task(c0_timestamp);

  // Webusb stuff
  if (webusb_output_enabled())
  {
    snapback_webcapture_task(c0_timestamp, &_analog_data_desnapped);
    webusb_input_report_task(c0_timestamp, &_analog_data_output, &_button_data_processed);
    _hoja_idle_state = false;
  }
  // Our communication core task
  else
  {
    hoja_comms_task(c0_timestamp, &_button_data_processed, &_analog_data_output);
  }

  #if (HOJA_CAPABILITY_BATTERY)
    _hoja_set_idle_state(&_button_data_processed, &_analog_data_output, c0_timestamp);
    // Always run monitor?
    battery_monitor_task(c0_timestamp);
  #endif

  // Handle HD rumble
  #if defined(HOJA_CONFIG_HDRUMBLE) && (HOJA_CONFIG_HDRUMBLE==1)
    hdrumble_hal_task(c0_timestamp);
  #endif

  watchdog_update();
}

volatile bool rumble_init_done = false;

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  static uint32_t c1_timestamp = 0;

  //cb_hoja_rumble_init();

  for (;;)
  {
    c1_timestamp = hoja_get_timestamp();

    // Check if we need to save
    settings_core1_save_check();

    // Do analog stuff :)
    analog_task(c1_timestamp);

    // Do IMU stuff
    imu_task(c1_timestamp);
  }
}

void hoja_init(hoja_config_t *config)
{
  // Stop if there's no config
  if (!config)
    return;

  input_mode_t _hoja_input_mode = 0;

  // Get our reboot reason
  hoja_reboot_memory_u reboot_mem = {.value = reboot_get_memory()};

  _hoja_hal_setup();
  _hoja_driver_setup();

  // Set up hardware first
  cb_hoja_hardware_setup();

#if ((HOJA_CAPABILITY_BLUETOOTH) == 1 || (HOJA_CAPABILITY_BATTERY == 1))
  // I2C Setup
  //i2c_init(HOJA_I2C_BUS, 400 * 1000);
  //gpio_set_function(HOJA_I2C_SDA, GPIO_FUNC_I2C);
  //gpio_set_function(HOJA_I2C_SCL, GPIO_FUNC_I2C);
#endif

  // Battery status should self-update

  // Test overclock
  set_sys_clock_khz(HOJA_SYS_CLK_HZ / 1000, true);

  // Read buttons to get a current state
  cb_hoja_read_buttons(&_button_data);

  // Load settings and related logic
  {
    bool settings_loaded = settings_load();

    if (!settings_loaded)
    {
      rgb_indicate(COLOR_ORANGE.color, 50);
    }

    analog_init(&_analog_data_input, &_analog_data_output, &_analog_data_desnapped, &_button_data);
    triggers_scale_init();
  }

  if (reboot_mem.reboot_reason == ADAPTER_REBOOT_REASON_BTSTART)
  {
    // We're rebooting from a BT start
    // We need to set the input method to BT
    _hoja_input_method = INPUT_METHOD_BLUETOOTH;
  }
  else if (reboot_mem.reboot_reason == ADAPTER_REBOOT_REASON_MODECHANGE)
  {
    _hoja_input_method = reboot_mem.gamepad_protocol;
    _hoja_input_mode = reboot_mem.gamepad_mode;
  }
  else
  {
    if (config->input_mode == INPUT_MODE_LOAD)
    {
      _hoja_input_mode = global_loaded_settings.input_mode;
    }
    else
    {
      _hoja_input_mode = config->input_mode;
    }

    _hoja_input_method = config->input_method;
  }

  // Macros for mode switching
  {
    if (_button_data.button_b)
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

  // Reset pairing if needed.
  if (_button_data.button_sync)
  {
    _hoja_input_mode |= 0b10000000; // Set pairing flag
    //global_loaded_settings.switch_host_address[0] = 0;
    //global_loaded_settings.switch_host_address[1] = 0;
    //global_loaded_settings.switch_host_address[2] = 0;
    //global_loaded_settings.switch_host_address[3] = 0;
    //global_loaded_settings.switch_host_address[4] = 0;
    //global_loaded_settings.switch_host_address[5] = 0;
  }

  // For switch Pro stuff
  switch_analog_calibration_init();

  rgb_mode_t rgbmode = global_loaded_settings.rgb_mode;
  uint8_t rgbbrightness = 100;
  uint32_t indicate_color = COLOR_WHITE.color;

  if (_hoja_input_method == INPUT_METHOD_AUTO)
  {
    int8_t plug_status = battery_get_plugged_status();

    if (plug_status == 0)
    {
      rgbbrightness = 70;
      _hoja_input_method = INPUT_METHOD_BLUETOOTH;
    }
    else
    {
      _hoja_input_method = INPUT_METHOD_USB;
    }
  }

  battery_set_source(PMIC_SOURCE_AUTO);

  // Checks for retro and modes where we don't care about
  // checking the plug status
  switch (_hoja_input_mode)
  {
  case INPUT_MODE_GCUSB:
    indicate_color = COLOR_CYAN.color;
    battery_set_charge_rate(250);
    break;
  case INPUT_MODE_XINPUT:
    indicate_color = COLOR_GREEN.color;
    battery_set_charge_rate(250);
    break;

  default:
  case INPUT_MODE_SWPRO:
    indicate_color = COLOR_WHITE.color;
    battery_set_charge_rate(250);
    break;

  case INPUT_MODE_DS4:
    // hoja_input_method = INPUT_METHOD_USB;
    indicate_color = COLOR_BLUE.color;
    battery_set_charge_rate(250);
    break;

  case INPUT_MODE_SNES:
    battery_set_charge_rate(50);
    _hoja_input_method = INPUT_METHOD_WIRED;
    rgbbrightness = 25;
    indicate_color = COLOR_RED.color;
    break;
  case INPUT_MODE_GAMECUBE:
    battery_set_charge_rate(50);
    _hoja_input_method = INPUT_METHOD_WIRED;
    rgbbrightness = 15;
    indicate_color = COLOR_PURPLE.color;
    break;
  case INPUT_MODE_N64:
    battery_set_charge_rate(0);
    _hoja_input_method = INPUT_METHOD_WIRED;
    rgbbrightness = 25;
    indicate_color = COLOR_YELLOW.color;
    break;
  }

  rgb_indicate(indicate_color, 50);
  rgb_init(rgbmode, rgbbrightness);
  // rgb_init(RGB_MODE_REACTIVE, rgbbrightness);

  hoja_comms_init(_hoja_input_mode, _hoja_input_method);

  if (_button_data.button_home)
  {
    global_loaded_settings.input_mode = _hoja_input_mode;
    settings_save_from_core0();
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
