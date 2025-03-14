#ifndef ANM_IDLE_H
#define ANM_IDLE_H

#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"

bool anm_idle_get_state(rgb_s *output);
bool anm_idle_handler(rgb_s* output);

#endif