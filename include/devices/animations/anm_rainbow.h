#ifndef ANM_RAINBOW_H
#define ANM_RAINBOW_H

#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"

bool anm_rainbow_get_state(rgb_s *output);
bool anm_rainbow_handler(rgb_s* output);

#endif