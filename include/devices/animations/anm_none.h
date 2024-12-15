#ifndef ANM_NONE_H
#define ANM_NONE_H
#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"

bool anm_none_get_state(rgb_s *output);
bool anm_none_handler(rgb_s* output);

#endif