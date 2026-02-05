#ifndef TRANSPORT_JB64_H
#define TRANSPORT_JB64_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"

bool transport_jb64_init(core_params_s *params);
void transport_jb64_task(uint64_t timestamp);

#endif