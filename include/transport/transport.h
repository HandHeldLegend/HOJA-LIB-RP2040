#ifndef TRANSPORT_TRANSPORT_H
#define TRANSPORT_TRANSPORT_H 

#include <stdbool.h>
#include <stdint.h>
#include "hoja_shared_types.h"

#include "cores/cores.h"

bool transport_init(core_params_s *params);
void transport_task(uint64_t timestamp);

#endif