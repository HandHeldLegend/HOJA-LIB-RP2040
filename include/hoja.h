#ifndef HOJA_H
#define HOJA_H

#define HOJA_BACKEND_VERSION 0x0001
#define HOJA_SETTINGS_VERSION 0xA002

#define HOJA_SYS_CLK_HZ 200000000

#include <stdbool.h>
#include <stdint.h>

#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"
#include "utilities/callback.h"
#include "devices/devices.h"
#include "devices/haptics.h"

typedef struct
{
    device_method_t  input_method;
    device_mode_t    input_mode;
} hoja_config_t;

void cb_hoja_baseband_update_loop(button_data_s *buttons);
uint16_t cb_hoja_hardware_test();

void cb_hoja_read_buttons(button_data_s *data);
bool cb_hoja_buttons_init();

// void cb_hoja_read_imu(imu_data_s *data_a, imu_data_s *data_b);

void cb_hoja_task_0_hook(uint32_t timestamp);
void cb_hoja_task_1_hook(uint32_t timestamp);
void cb_hoja_rumble_init();
void cb_hoja_set_bluetooth_enabled(bool enable);
void cb_hoja_set_uart_enabled(bool enable);
void cb_hoja_rumble_test();
uint8_t cb_hoja_get_battery_level();

bool hoja_get_idle_state();
void hoja_set_baseband_update(bool set);
void hoja_get_rumble_settings(uint8_t *intensity, rumble_type_t *type);
void hoja_shutdown_instant();
device_method_t hoja_get_input_method();
analog_data_s *hoja_get_desnapped_analog_data();
button_data_s *hoja_get_raw_button_data();
uint32_t hoja_get_timestamp();
void hoja_deinit(callback_t cb);
void hoja_shutdown();

void hoja_init();

void hoja_load_remap(button_remap_s *remap_profile);

void hoja_setup_gpio_scan(uint8_t gpio);
void hoja_setup_gpio_push(uint8_t gpio);
void hoja_setup_gpio_button(uint8_t gpio);

#endif
