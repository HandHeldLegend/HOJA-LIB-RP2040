#ifndef HOJA_H
#define HOJA_H

#include "hoja_includes.h"
#include "interval.h"

void cb_hoja_hardware_setup();
void cb_hoja_read_buttons(button_data_s *data);
void cb_hoja_read_analog(a_data_s *data);
void cb_hoja_read_imu(imu_data_s *data_a, imu_data_s *data_b);

void cb_hoja_rumble_enable(bool rumble);

void cb_hoja_task_1_hook(uint32_t timestamp);

void hoja_init();

void hoja_load_remap(button_remap_s *remap_profile);

void hoja_setup_gpio_scan(uint8_t gpio);
void hoja_setup_gpio_push(uint8_t gpio);
void hoja_setup_gpio_button(uint8_t gpio);

#endif
