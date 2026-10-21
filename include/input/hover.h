#ifndef INPUT_HOVER_H
#define INPUT_HOVER_H

// This is HOJAs new input system called HOVER
// HOVER is designed to allow a dynamic array of inputs
// that can be analog or digital in nature. It also manages
// input thresholds, deadzone, and the ability to implement rapid trigger

#include <stdint.h>
#include <stdbool.h>
#include "input_shared_types.h"
#include "settings_shared_types.h"

void hover_task(uint64_t timestamp);
mapper_input_s hover_access_boot();
void hover_access_safe(mapper_input_s *out);
void hover_init();
void hover_config_command(uint8_t cmd, webreport_cmd_confirm_t cb);

// True when a trigger (0 left, 1 right) is fitted as mechanical: chosen by the owner, or found by
// auto detection. Always false on boards without HOJA_MECHANICAL_TRIGGERS.
bool hover_trigger_mechanical(uint8_t side);

#endif