#ifndef TRANSPORT_NESBUS_H
#define TRANSPORT_NESBUS_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"

bool transport_nesbus_init(core_params_s *params);
void transport_nesbus_task(uint64_t timestamp);

#endif