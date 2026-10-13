#ifndef MACRO_PAIRING_H
#define MACRO_PAIRING_H

#include "input_shared_types.h"
#include "input/mapper.h"
#include <stdint.h>

void macro_pairing(uint64_t timestamp, mapper_input_s *buttons);

// The configured pairing combo: whether there is one, and whether it is held now
bool macro_pairing_available(void);
bool macro_pairing_combo_pressed(const mapper_input_s *input);

#endif