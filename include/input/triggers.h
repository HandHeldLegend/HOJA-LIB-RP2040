#ifndef INPUT_TRIGGERS_H
#define INPUT_TRIGGERS_H

#include <stdbool.h>
#include <stdint.h>

#include "settings_shared_types.h"
#include "input_shared_types.h"

void triggers_config_cmd(trigger_cmd_t cmd, command_confirm_t cb);
bool triggers_init();
void triggers_process_pre(int l_type, int r_type, button_data_s *safe);
void triggers_process_post(int l_type, int r_type, button_data_s *safe);

#endif