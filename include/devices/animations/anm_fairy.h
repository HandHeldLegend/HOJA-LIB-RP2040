#ifndef ANM_FAIRY_H
#define ANM_FAIRY_H
#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"

bool anm_fairy_get_state(rgb_s *output);
bool anm_fairy_handler(rgb_s* output);

#endif