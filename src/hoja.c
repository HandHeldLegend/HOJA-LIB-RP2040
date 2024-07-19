#include "hoja.h"

#define CENTER 2048

button_data_s _button_data = {0};
button_data_s _button_data_processed = {0};
button_data_s _button_data_output = {0};

// The raw input analog data
a_data_s _analog_data_input = {0};

// The buffered analog data
// This is the analog data minus snapback for checking in the app
a_data_s _analog_data_desnapped = {0};

// This is the outgoing analog data
a_data_s _analog_data_output = {0};

button_remap_s *_hoja_remap = NULL;
input_method_t _hoja_input_method = INPUT_METHOD_AUTO;

uint32_t _timer_owner_0;
uint32_t _timer_owner_1;
uint32_t _timer_owner_2;
auto_init_mutex(_hoja_timer_mutex);

volatile uint32_t _hoja_timestamp = 0;

bool _baseband_loop = false;

hoja_rumble_msg_s _hoja_rumble_msg_left   = {0};
hoja_rumble_msg_s _hoja_rumble_msg_right  = {0};

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

__attribute__((weak)) void cb_hoja_rumble_init()
{
}


// This will automatically cycle through the appropriate
// rumble frames that are set
void hoja_rumble_task(uint32_t timestamp)
{
  return;
  static uint32_t t = 0;
  static interval_s rumble_interval = {0};
  static uint8_t frame_idx = 0;

  #define R_INTERVAL_1 8000
  #define R_INTERVAL_2 (R_INTERVAL_1 / 2)
  #define R_INTERVAL_3 (R_INTERVAL_1 / 3)
  #define R_INTERVAL_0 1000

  static uint32_t period  = R_INTERVAL_1;

  switch (_hoja_rumble_msg_left.count)
  {
  default:
  case 0:
    period = R_INTERVAL_0;
    break;

  case 1:
    period = R_INTERVAL_1;
    break;

  case 2:
    period = R_INTERVAL_2;
    break;

  case 3:
    period = R_INTERVAL_3;
    break;
  }

  if(interval_run(timestamp, period, &rumble_interval))
  {
    if(_hoja_rumble_msg_left.unread)
    {
      frame_idx = 0;
      _hoja_rumble_msg_left.unread = false;
    }

    if(frame_idx < _hoja_rumble_msg_left.count)
    {
      // This is sent to app-space to adjust the frequency/amplitude
      cb_hoja_rumble_set(&(_hoja_rumble_msg_left.frames[frame_idx]), &(_hoja_rumble_msg_right.frames[frame_idx]));
    }
    frame_idx++;
  }
}

// Called by other components in the HOJA api
void hoja_rumble_set(hoja_rumble_msg_s *left, hoja_rumble_msg_s *right)
{
  memcpy(&_hoja_rumble_msg_left, left, sizeof(hoja_rumble_msg_s));
  _hoja_rumble_msg_left.unread  = true;
  memcpy(&_hoja_rumble_msg_right, right, sizeof(hoja_rumble_msg_s));
  _hoja_rumble_msg_right.unread = true;
}


__attribute__((weak)) void cb_hoja_rumble_set(hoja_haptic_frame_s *left, hoja_haptic_frame_s *right)
{
  (void *)left;
  (void *)right;
}

__attribute__((weak)) void cb_hoja_rumble_test()
{
}

__attribute__((weak)) void cb_hoja_task_1_hook(uint32_t timestamp)
{
  (void)timestamp;
}

__attribute__((weak)) void cb_hoja_task_0_hook(uint32_t timestamp)
{
  (void)timestamp;
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

a_data_s *hoja_get_desnapped_analog_data()
{
  return &_analog_data_desnapped;
}

button_data_s *hoja_get_raw_button_data()
{
  return &_button_data;
  ;
}

bool _hoja_idle_state = false;
#define IDLE_TME_SECONDS 25
void _hoja_set_idle_state(button_data_s *buttons, a_data_s *analogs, uint32_t timestamp)
{
  static interval_s interval = {0};
  static interval_s check_interval = {0};
  const uint32_t idle_time = IDLE_TME_SECONDS * 1000 * 1000;
  static uint16_t btns = 0;
  static a_data_s last_analogs = {0};

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
  if (mutex_try_enter(&_hoja_timer_mutex, &_timer_owner_2))
  {
    t = _hoja_timestamp;
    mutex_exit(&_hoja_timer_mutex);
  }
  return t;
}

void hoja_shutdown()
{
  static bool _shutdown_started = false;
  if (_shutdown_started)
    return;

  _shutdown_started = true;
#if (HOJA_CAPABILITY_RGB == 1 && HOJA_CAPABILITY_BATTERY == 1)
  rgb_shutdown_start(false);
#elif (HOJA_CAPABILITY_BATTERY == 1)
  util_battery_enable_ship_mode();
#else
  hoja_shutdown_instant();
#endif
}

bool _watchdog_enabled = false;

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  if (!_watchdog_enabled)
  {
    watchdog_enable(7500, false);
    _watchdog_enabled = true;
  }

  if (mutex_try_enter(&_hoja_timer_mutex, &_timer_owner_0))
  {
    _hoja_timestamp = time_us_32();
    mutex_exit(&_hoja_timer_mutex);
  }

  // Read buttons
  cb_hoja_read_buttons(&_button_data);

  if (_baseband_loop)
  {
    cb_hoja_baseband_update_loop(&_button_data);
    watchdog_update();
    rgb_task(_hoja_timestamp);
    util_battery_monitor_task_usb(_hoja_timestamp);
    return;
  }

  remap_buttons_task();
  macro_handler_task(_hoja_timestamp, &_button_data);

  rgb_task(_hoja_timestamp);

  hoja_rumble_task(_hoja_timestamp);

  // Webusb stuff
  if (webusb_output_enabled())
  {
    snapback_webcapture_task(_hoja_timestamp, &_analog_data_desnapped);
    webusb_input_report_task(_hoja_timestamp, &_analog_data_output, &_button_data_processed);
    _hoja_idle_state = false;
  }
  // Our communication core task
  else
  {
    hoja_comms_task(_hoja_timestamp, &_button_data_processed, &_analog_data_output);
  }

#if (HOJA_CAPABILITY_BATTERY)
  _hoja_set_idle_state(&_button_data_processed, &_analog_data_output, _hoja_timestamp);
#endif

  if (_hoja_input_method == INPUT_METHOD_USB)
  {
    util_battery_monitor_task_usb(_hoja_timestamp);
  }

  watchdog_update();
  cb_hoja_task_0_hook(_hoja_timestamp);
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

  input_mode_t _hoja_input_mode = 0;

  // Get our reboot reason
  hoja_reboot_memory_u reboot_mem = {.value = reboot_get_memory()};

  // Set up hardware first
  cb_hoja_hardware_setup();

#if ((HOJA_CAPABILITY_BLUETOOTH) == 1 || (HOJA_CAPABILITY_BATTERY == 1))
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
      rgb_indicate(COLOR_ORANGE.color, 50);
    }

    analog_init(&_analog_data_input, &_analog_data_output, &_analog_data_desnapped, &_button_data);
    triggers_scale_init();
  }

  // Reset pairing if needed.
  if (_button_data.button_sync)
  {
    memset(global_loaded_settings.switch_host_address, 0, 6);
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

  // Initialize rumble
  cb_hoja_rumble_init();

  // For switch Pro stuff
  switch_analog_calibration_init();

  rgb_mode_t rgbmode = global_loaded_settings.rgb_mode;
  uint8_t rgbbrightness = 100;
  uint32_t indicate_color = COLOR_WHITE.color;

  if (_hoja_input_method == INPUT_METHOD_AUTO)
  {
    if (!util_wire_connected())
    {

      rgbbrightness = 70;
      _hoja_input_method = INPUT_METHOD_BLUETOOTH;
    }
    else
    {
      _hoja_input_method = INPUT_METHOD_USB;
    }
  }

#define TEST_OPTION 0

#if (TEST_OPTION == 0)
  util_battery_set_source(PMIC_SOURCE_AUTO);
  util_battery_set_charge_rate(250);
#elif (TEST_OPTION == 1)
  util_battery_set_source(PMIC_SOURCE_AUTO);
  util_battery_set_charge_rate(0);
#elif (TEST_OPTION == 2)
  util_battery_set_source(PMIC_SOURCE_BAT);
#elif (TEST_OPTION == 3)
  util_battery_set_source(PMIC_SOURCE_BAT);
  util_battery_set_charge_rate(0);
#endif

  // Checks for retro and modes where we don't care about
  // checking the plug status
  switch (_hoja_input_mode)
  {
  case INPUT_MODE_GCUSB:
    indicate_color = COLOR_CYAN.color;
    //_hoja_input_method = INPUT_METHOD_USB;
    break;
  case INPUT_MODE_XINPUT:
    indicate_color = COLOR_GREEN.color;
    break;

  default:
  case INPUT_MODE_SWPRO:
    indicate_color = COLOR_WHITE.color;
    break;

  case INPUT_MODE_DS4:
    // hoja_input_method = INPUT_METHOD_USB;
    indicate_color = COLOR_BLUE.color;
    break;

  case INPUT_MODE_SNES:
    util_battery_set_charge_rate(0);
    _hoja_input_method = INPUT_METHOD_WIRED;
    rgbbrightness = 25;
    indicate_color = COLOR_RED.color;
    break;
  case INPUT_MODE_GAMECUBE:
    util_battery_set_charge_rate(0);
    _hoja_input_method = INPUT_METHOD_WIRED;
    rgbbrightness = 15;
    indicate_color = COLOR_PURPLE.color;
    break;
  case INPUT_MODE_N64:
    util_battery_set_charge_rate(0);
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
