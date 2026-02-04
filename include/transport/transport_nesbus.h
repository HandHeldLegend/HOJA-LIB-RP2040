#ifndef TRANSPORT_NESBUS_H
#define TRANSPORT_NESBUS_H

#include <stdbool.h>
#include <stdint.h>

bool transport_nesbus_init();
void transport_nesbus_task(uint64_t timestamp);

#endif