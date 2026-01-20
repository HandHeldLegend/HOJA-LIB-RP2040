#ifndef HOJA_H
#define HOJA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "hoja_shared_types.h"
#include "input_shared_types.h"
#include "devices_shared_types.h"

#include "input/analog.h"
#include "utilities/callback.h"

void cb_hoja_shutdown();
bool cb_hoja_boot(boot_input_s *boot);
uint16_t cb_hoja_read_battery();
// Accessible as a uint16_t array of size 4 only
void cb_hoja_read_joystick(uint16_t *input);
void cb_hoja_read_input(mapper_input_s *input);
void cb_hoja_init();

void hoja_set_debug_data(uint8_t data);

void hoja_set_connected_status(connection_status_t status);
void hoja_set_notification_status(rgb_s color);
void hoja_set_ss_notif(rgb_s color);
void hoja_clr_ss_notif();

hoja_status_s hoja_get_status();

void hoja_restart();
void hoja_shutdown();
void hoja_deinit(callback_t cb);
void hoja_init();

#endif
