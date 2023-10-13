#ifndef HOJA_H
#define HOJA_H

#include "hoja_includes.h"
#include "interval.h"

void cb_hoja_hardware_setup();
void cb_hoja_read_buttons(button_data_s *data);
void cb_hoja_read_analog(a_data_s *data);
void cb_hoja_read_imu(imu_data_s *data_a, imu_data_s *data_b);
void cb_hoja_rumble_enable(float intensity);
void cb_hoja_task_1_hook(uint32_t timestamp);
void cb_hoja_set_rumble_intensity(uint8_t floor, uint8_t intensity);

void hoja_init(hoja_config_t *config);

void hoja_load_remap(button_remap_s *remap_profile);

void hoja_setup_gpio_scan(uint8_t gpio);
void hoja_setup_gpio_push(uint8_t gpio);
void hoja_setup_gpio_button(uint8_t gpio);

#endif
