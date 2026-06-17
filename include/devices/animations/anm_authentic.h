#ifndef ANM_AUTHENTIC_H
#define ANM_AUTHENTIC_H

#include "devices_shared_types.h"
#include <stdbool.h>

bool anm_authentic_get_state(rgb_s *output);
bool anm_authentic_handler(rgb_s *output);

// Rebuild group colors from gamepad mode + input profile + reactive map.
void anm_authentic_refresh(void);

#endif
