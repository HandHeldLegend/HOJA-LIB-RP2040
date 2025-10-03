#ifndef UTILITIES_SYSMON_H
#define UTILITIES_SYSMON_H

#include <stdint.h>
#include <stdbool.h>

void sysmon_task(uint64_t timestamp);

// Call this if the system needs to be shut down with a warning
void sysmon_set_critical_shutdown(void);
#endif