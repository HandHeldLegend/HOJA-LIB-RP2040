#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "settings_shared_types.h"
#include "input_shared_types.h"
#include "hoja_shared_types.h"

#define MAPPER_BUTTON_DOWN(presses, code) (presses[code] ? true : false)

void dpad_translate_input(bool digital[4]);
int mapper_joystick_concat(int center, int neg, int pos);
void mapper_config_command(mapper_cmd_t cmd, webreport_cmd_confirm_t cb);

void mapper_init();
const inputConfigSlot_s *mapper_get_active_profile(void);
core_reportformat_t mapper_get_palette_format(void);
void mapper_webusb_remap_preview_end(void);
// Select which Wii profile (Nunchuk / Classic Pro / Sideways) the mapper reads in Wii mode.
void mapper_set_wii_profile(mapper_wii_profile_t profile);
mapper_input_s mapper_get_translated_input();
mapper_input_s mapper_get_input();
// Hold every output at rest (buttons up, sticks centred) while the presses mean something else,
// such as a BLE config code being entered
void mapper_hold_neutral(bool hold);
// Hold only what the game receives at rest (the config app's live view still sees every input);
// the app's "send inputs to game" switch
void mapper_hold_game_inputs(bool hold);
bool mapper_game_inputs_held(void);
// The config app said something: keeps a hold alive (it lapses after 5 s of silence)
void mapper_app_heard(void);

#endif