#ifndef MACRO_PCMDEBUG_H
#define MACRO_PCMDEBUG_H

#include "input_shared_types.h"
#include <stdint.h>
#include <stdbool.h>

void macro_pcmdebug(uint64_t timestamp, button_data_s *buttons);

#endif