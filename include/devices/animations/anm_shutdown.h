#ifndef ANM_SHUTDOWN_H
#define ANM_SHUTDOWN_H
#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"
#include "utilities/callback.h"

void anm_shutdown_set_cb(callback_t cb);
bool anm_shutdown_handler(rgb_s *output);
bool anm_shutdown_get_state(rgb_s *output);

#endif