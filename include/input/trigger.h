#ifndef INPUT_TRIGGERS_H
#define INPUT_TRIGGERS_H

#include <stdbool.h>
#include <stdint.h>

#include "settings_shared_types.h"
#include "input_shared_types.h"

typedef enum 
{
    TRIGGER_ACCESS_RAW_DATA,
    TRIGGER_ACCESS_SCALED_DATA,
} trigger_access_t;

void trigger_access_safe(trigger_data_s *out, trigger_access_t type);
void trigger_task(uint64_t timestamp);

void trigger_config_cmd(trigger_cmd_t cmd, webreport_cmd_confirm_t cb);
bool trigger_init();

void trigger_gc_process(button_data_s *b_state, trigger_data_s *t_state);

#endif