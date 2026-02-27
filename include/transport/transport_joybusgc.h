#ifndef TRANSPORT_JBGC_H
#define TRANSPORT_JBGC_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"
#include "transport/transport.h"

bool transport_jbgc_autoinit(transport_autoinit_state_t *sm, core_params_s *params);
bool transport_jbgc_init(core_params_s *params);
void transport_jbgc_task(uint64_t timestamp);

#endif