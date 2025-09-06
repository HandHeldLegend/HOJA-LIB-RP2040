#ifndef IDLE_MANAGER_H
#define IDLE_MANAGER_H

#include <stdint.h>

void idle_manager_heartbeat();
void idle_manager_task(uint64_t timestamp);

#endif