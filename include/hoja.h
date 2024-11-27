#ifndef HOJA_H
#define HOJA_H

#define HOJA_BACKEND_VERSION 0x0001
#define HOJA_SETTINGS_VERSION 0xA002

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "hoja_shared_types.h"
#include "input_shared_types.h"

#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"
#include "utilities/callback.h"

void cb_hoja_read_buttons(button_data_s *data);
bool cb_hoja_buttons_init();

gamepad_mode_t hoja_gamepad_mode_get();

void hoja_shutdown();
void hoja_deinit(callback_t cb);
void hoja_init();

void hoja_setup_gpio_scan(uint8_t gpio);
void hoja_setup_gpio_push(uint8_t gpio);
void hoja_setup_gpio_button(uint8_t gpio);

#endif
