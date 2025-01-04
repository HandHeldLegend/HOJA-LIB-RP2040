#ifndef HOJA_H
#define HOJA_H

#define HOJA_SETTINGS_VERSION 0xA003

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "hoja_shared_types.h"
#include "input_shared_types.h"
#include "devices_shared_types.h"

#include "input/button.h"
#include "input/analog.h"
#include "input/remap.h"
#include "utilities/callback.h"

void cb_hoja_read_buttons(button_data_s *data);
bool cb_hoja_buttons_init();
bool hoja_get_running_status();
void hoja_set_connected_status(int status);
int  hoja_get_connected_status();
void hoja_set_player_number_status(int player_number);
int  hoja_get_player_number_status();

rgb_s hoja_gamepad_mode_color_get();
gamepad_mode_t hoja_gamepad_mode_get();

void hoja_restart();
void hoja_shutdown();
void hoja_deinit(callback_t cb);
void hoja_init();

#endif
