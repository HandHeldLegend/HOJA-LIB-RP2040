#ifndef HOJA_H
#define HOJA_H

#define HOJA_BACKEND_VERSION 0x0001
#define HOJA_SETTINGS_VERSION 0xA001

#define HOJA_HAPTIC_BASE_FREQ 40

#include "hoja_includes.h"
#include "interval.h"

uint16_t cb_hoja_hardware_test();
void cb_hoja_hardware_setup();
void cb_hoja_read_buttons(button_data_s *data);
void cb_hoja_read_analog(a_data_s *data);
void cb_hoja_read_imu(imu_data_s *data_a, imu_data_s *data_b);
void cb_hoja_rumble_set(float frequency, float amplitude);
void cb_hoja_task_1_hook(uint32_t timestamp);
void cb_hoja_rumble_init();
void cb_hoja_set_bluetooth_enabled(bool enable);
void cb_hoja_set_uart_enabled(bool enable);

void hoja_get_rumble_intensity(uint8_t *lower, uint8_t *upper);
void hoja_shutdown_instant();
input_method_t hoja_get_input_method();
a_data_s *hoja_get_desnapped_analog_data();
button_data_s *hoja_get_raw_button_data();
void hoja_shutdown();
void hoja_init(hoja_config_t *config);

void hoja_load_remap(button_remap_s *remap_profile);

void hoja_setup_gpio_scan(uint8_t gpio);
void hoja_setup_gpio_push(uint8_t gpio);
void hoja_setup_gpio_button(uint8_t gpio);

#endif
