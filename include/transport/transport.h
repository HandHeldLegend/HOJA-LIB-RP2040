#ifndef TRANSPORT_TRANSPORT_H
#define TRANSPORT_TRANSPORT_H 

#include <stdbool.h>
#include <stdint.h>
#include "hoja_shared_types.h"

bool transport_init(gamepad_transport_t transport);
void transport_task(uint64_t timestamp);

#endif