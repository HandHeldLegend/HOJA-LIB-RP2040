#ifndef HOJA_H
#define HOJA_H

#if __has_include("timestamp.h")
    #include "timestamp.h"
#endif

#ifndef BUILD_TIMESTAMP
    #define FIRMWARE_VERSION_TIMESTAMP 0x00000000
#else 
    #define FIRMWARE_VERSION_TIMESTAMP BUILD_TIMESTAMP
#endif

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

void hoja_set_connected_status(int status);
void hoja_set_player_number_status(int player_number);
void hoja_set_notification_status(rgb_s color);

hoja_status_s hoja_get_status();

void hoja_restart();
void hoja_shutdown();
void hoja_deinit(callback_t cb);
void hoja_init();

#endif
