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

uint32_t _timer_owner_0;
uint32_t _timer_owner_1;
auto_init_mutex(_hoja_timer_mutex);

volatile uint32_t _hoja_timestamp = 0;

// USER DEFINED CALLBACKS
// DO NOT EDIT
__attribute__ ((weak)) void cb_hoja_hardware_setup()
{
}

__attribute__ ((weak)) void cb_hoja_read_buttons(button_data_s *data)
{
  (void) &data;
}

__attribute__ ((weak)) void cb_hoja_read_analog(a_data_s *data)
{
  (void) &data;
}

__attribute__ ((weak)) void cb_hoja_read_imu(imu_data_s *data_a, imu_data_s *data_b)
{
  (void) &data_a;
  (void) &data_b;
}

__attribute__ ((weak)) void cb_hoja_rumble_enable(float intensity)
{
  (void) intensity;
}

__attribute__ ((weak)) void cb_hoja_task_1_hook(uint32_t timestamp)
{
  (void) timestamp;
}

__attribute__ ((weak)) void cb_hoja_set_rumble_intensity(uint8_t intensity)
{
  (void) intensity;
}

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  if(mutex_try_enter(&_hoja_timer_mutex, &_timer_owner_0))
  {
    _hoja_timestamp = time_us_32();
    mutex_exit(&_hoja_timer_mutex);
  }
  
  // Read buttons
  cb_hoja_read_buttons(&_button_data);
  safe_mode_task(_hoja_timestamp, &_button_data);
  remap_buttons_task();

  // Webusb stuff
  if(webusb_output_enabled())
  {
    snapback_webcapture_task(_hoja_timestamp, &_analog_data_buffered);
    webusb_input_report_task(_hoja_timestamp, &_analog_data_buffered);
  }

  hoja_comms_task(_hoja_timestamp, &_button_data_processed, &_analog_data_output);
}

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  for (;;)
  {
    if(mutex_try_enter(&_hoja_timer_mutex, &_timer_owner_1))
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

    rgb_task(_hoja_timestamp);

    // Do callback for userland code insertion
    cb_hoja_task_1_hook(_hoja_timestamp);
  }
}

void hoja_init()
{
  // Set up hardware first
  cb_hoja_hardware_setup();

  rgb_init();
  
  // Read buttons to get a current state
  cb_hoja_read_buttons(&_button_data);

  // Load settings and related logic
  {
    bool settings_loaded = settings_load();

    if (!settings_loaded)
    {
      settings_reset_to_default();
      sleep_ms(200);
      rgb_load_preset((rgb_preset_t *) &global_loaded_settings.rgb_colors[0]);
      
      analog_init(&_analog_data_input, &_analog_data_output, &_analog_data_buffered, &_button_data);
    }
    else
    {
      rgb_load_preset((rgb_preset_t *) &global_loaded_settings.rgb_colors[0]);
      
      analog_init(&_analog_data_input, &_analog_data_output, &_analog_data_buffered, &_button_data);
    }
  }

  // Set rumble intensity
  cb_hoja_set_rumble_intensity(global_loaded_settings.rumble_intensity);

  // For switch Pro stuff
  switch_analog_calibration_init();

  input_mode_t _hoja_input_mode = 0;

  _hoja_input_mode = global_loaded_settings.input_mode;

  if (_button_data.button_x)
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

  switch(_hoja_input_mode)
  {
    case INPUT_MODE_GCUSB:
    case INPUT_MODE_XINPUT:
      break;

    default:
    case INPUT_MODE_SWPRO:
      rgb_preset_reload();
      break;

    case INPUT_MODE_SNES:
    case INPUT_MODE_GAMECUBE:
    case INPUT_MODE_N64:
      rgb_set_brightness(10);
      rgb_preset_reload();
      break;
  }

  rgb_set_dirty();

  if(_button_data.button_home)
  {
    global_loaded_settings.input_mode = _hoja_input_mode;
    settings_save();
  }

  // Initialize button remapping
  remap_init(_hoja_input_mode, &_button_data, &_button_data_processed);

  hoja_comms_init(_hoja_input_mode);

  // Enable lockout victimhood :,)
  multicore_lockout_victim_init();

  // Launch second core
  multicore_launch_core1(_hoja_task_1);
  // Launch first core
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
