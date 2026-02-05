#ifndef TRANSPORT_BT_H
#define TRANSPORT_BT_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"

bool transport_bt_init(core_params_s *params);
void transport_bt_task(uint64_t timestamp);

#endif