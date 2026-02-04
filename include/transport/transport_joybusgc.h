#ifndef TRANSPORT_JBGC_H
#define TRANSPORT_JBGC_H

#include <stdbool.h>
#include <stdint.h>

bool transport_jbgc_init();
void transport_jbgc_task(uint64_t timestamp);

#endif