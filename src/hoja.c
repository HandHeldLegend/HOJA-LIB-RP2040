#include "hoja.h"

bool _hoja_usb_task_enable = false;

button_data_s _button_data = {0};
button_data_s _button_data_processed = {0};

a_data_s _analog_data = {0};
a_data_s _analog_data_processed = {0};

button_remap_s *_hoja_remap = NULL;

volatile uint32_t _hoja_timestamp = 0;

bool _remap_enabled = false;

// Core 0 task loop entrypoint
void _hoja_task_0()
{
  _hoja_timestamp = time_us_32();

  // Read buttons
  cb_hoja_read_buttons(&_button_data);
  safe_mode_task(&_button_data);
  remap_buttons_task();

  if (_hoja_usb_task_enable)
  {
    // Process USB if needed
    tud_task();
    snapback_webcapture_task(_hoja_timestamp, &_analog_data_processed);
    hoja_usb_task(_hoja_timestamp, &_button_data_processed, &_analog_data_processed);
  }
  {
    hoja_comms_task(_hoja_timestamp, &_button_data_processed, &_analog_data_processed);
  }
}

// Core 1 task loop entrypoint
void _hoja_task_1()
{
  for (;;)
  {
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

void hoja_remapping_enable(bool enable)
{
  _remap_enabled = enable;
}

void hoja_init()
{
  // Set up hardware first
  cb_hoja_hardware_setup();

  // Initialize RGB and set one color
  {
    rgb_init();

    rgb_s c = {
        .r = 150,
        .g = 128,
        .b = 200,
    };
    rgb_set_all(c.color);
    rgb_set_dirty();
  }
  
  // Read buttons to get a current state
  cb_hoja_read_buttons(&_button_data);

  // Load settings and related logic
  {
    bool settings_loaded = settings_load();

    if (!settings_loaded)
    {
      settings_reset_to_default();
      sleep_ms(200);
      analog_init(&_analog_data, &_analog_data_processed, &_button_data);
    }
    else
    {
      rgb_load_preset();
      rgb_set_dirty();
      analog_init(&_analog_data, &_analog_data_processed, &_button_data);
    }
  }

  // For switch Pro stuff
  switch_analog_calibration_init();

  _hoja_usb_task_enable = true;
  input_mode_t _hoja_input_mode = 0;

  if (_button_data.button_x)
  {
    _hoja_input_mode = INPUT_MODE_XINPUT;
  }
  else if (_button_data.button_a)
  {
    _hoja_usb_task_enable = false;
    _hoja_input_mode = INPUT_MODE_GAMECUBE;
  }
  else if (_button_data.button_b)
  {
    _hoja_usb_task_enable = false;
    _hoja_input_mode = INPUT_MODE_N64;
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
