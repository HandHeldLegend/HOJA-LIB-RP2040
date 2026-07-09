#ifndef UTILITIES_SYSMON_H
#define UTILITIES_SYSMON_H

#include <stdint.h>
#include <stdbool.h>

#define SYSMON_TASK_INTERVAL_US (1 * 1000 * 1000)

void sysmon_init();
void sysmon_shutdown();
void sysmon_task(uint64_t timestamp);
// Call this if the system needs to be shut down with a warning
void sysmon_set_critical_shutdown(void);
#endif