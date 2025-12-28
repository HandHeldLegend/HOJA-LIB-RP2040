#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "settings_shared_types.h"
#include "input_shared_types.h"

#define MAPPER_BUTTON_DOWN(presses, code) (presses[code] ? true : false)

int mapper_joystick_concat(int center, int neg, int pos);
void mapper_config_command(mapper_cmd_t cmd, webreport_cmd_confirm_t cb);

void mapper_init();
mapper_input_s mapper_get_translated_input();
mapper_input_s mapper_get_input();

#endif