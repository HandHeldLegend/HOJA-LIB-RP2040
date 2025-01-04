#ifndef ANM_REACT_H
#define ANM_REACT_H

#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"

bool anm_react_get_state(rgb_s *output);
bool anm_react_handler(rgb_s* output);

#endif