#ifndef HOJA_H
#define HOJA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "hoja_shared_types.h"
#include "input_shared_types.h"
#include "devices_shared_types.h"

#include "devices/battery.h"

#include "input/analog.h"
#include "utilities/callback.h"

// Board-supplied driver/configuration bundle.
// Boards populate this (typically a const instance) and register it from
// cb_hoja_init() via hoja_config_set(). Device layers pull their assigned
// driver from here during init. Fields are added per-subsystem as drivers
// migrate to the injected-driver model.
typedef struct
{
    const battery_driver_s *battery_driver;

    // Board-specific battery pack capacity in mAh. Generic (not tied to any
    // particular PMIC/fuel-gauge driver), so it lives directly in the config.
    uint16_t battery_capacity_mah;
} hoja_config_s;

const hoja_config_s *hoja_config_get(void);

void cb_hoja_shutdown();
bool cb_hoja_boot(boot_input_s *boot);
// Optional: if true, *mode_out is applied for face-button mode selection (use GAMEPAD_MODE_LOAD
// to defer to stored default). If false, the library uses digital and/or built-in analog face logic.
bool cb_hoja_boot_custom_face_mode(const mapper_input_s *in, gamepad_mode_t *mode_out);
uint16_t cb_hoja_read_battery();
// Accessible as a uint16_t array of size 4 only
void cb_hoja_read_joystick(uint16_t *input);
void cb_hoja_read_input(mapper_input_s *input);
void cb_hoja_init();

void hoja_set_debug_data(uint8_t data);

void hoja_set_player_number(uint8_t number);
void hoja_set_connected_status(connection_status_t status);
void hoja_set_notification_status(rgb_s color);
void hoja_set_ss_notif(rgb_s color);
void hoja_clr_ss_notif();

hoja_status_s hoja_get_status();

void hoja_restart();
void hoja_shutdown();
void hoja_deinit(callback_t cb);
void hoja_init(const hoja_config_s *config);

#endif
