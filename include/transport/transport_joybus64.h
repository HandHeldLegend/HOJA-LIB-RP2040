#ifndef TRANSPORT_JB64_H
#define TRANSPORT_JB64_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"
#include "transport/transport.h"

bool transport_jbgc_autoinit(transport_autoinit_state_t *sm, core_params_s *params);
bool transport_jb64_init(core_params_s *params);
void transport_jb64_task(uint64_t timestamp);

#endif