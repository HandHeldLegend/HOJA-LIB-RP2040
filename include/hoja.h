#ifndef HOJA_H
#define HOJA_H

#define HOJA_SETTINGS_VERSION 0xA003

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

#endif
