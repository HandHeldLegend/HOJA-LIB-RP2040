#ifndef IDLE_MANAGER_H
#define IDLE_MANAGER_H

#include <stdint.h>

#define IDLE_TASK_INTERVAL_US (1 * 1000 * 1000)

void idle_manager_heartbeat();
void idle_manager_task(uint64_t timestamp);

#endif