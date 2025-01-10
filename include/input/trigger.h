#ifndef INPUT_TRIGGERS_H
#define INPUT_TRIGGERS_H

#include <stdbool.h>
#include <stdint.h>

#include "settings_shared_types.h"
#include "input_shared_types.h"

typedef enum 
{
    TRIGGER_ACCESS_RAW_DATA,
    TRIGGER_ACCESS_SCALED_DATA
} trigger_access_t;

bool trigger_access_try(trigger_data_s *out, trigger_access_t type);
void trigger_task(uint32_t timestamp);

void trigger_config_cmd(trigger_cmd_t cmd, command_confirm_t cb);
bool trigger_init();

void trigger_process_pre(int l_type, int r_type, button_data_s *safe);
void trigger_process_post(int l_type, int r_type, button_data_s *safe);

#endif