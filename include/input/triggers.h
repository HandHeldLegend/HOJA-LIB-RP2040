#ifndef INPUT_TRIGGERS_H
#define INPUT_TRIGGERS_H

#include <stdbool.h>
#include <stdint.h>

#include "settings_shared_types.h"
#include "input_shared_types.h"

void triggers_config_cmd(trigger_cmd_t cmd, const uint8_t *data, setting_callback_t cb);
bool triggers_init();
void triggers_process(button_data_s *in, button_data_s *out);

#endif