#ifndef INPUT_MACROS_H
#define INPUT_MACROS_H

#include <stdint.h>
#include "input/input.h"

void macro_handler_task(uint32_t timestamp, button_data_s *in);

bool macro_safe_mode_check();

#endif