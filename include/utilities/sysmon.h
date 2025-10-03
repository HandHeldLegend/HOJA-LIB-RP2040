#ifndef HOJA_SYSMON_H
#define HOJA_SYSMON_H

#include "devices_shared_types.h"
#include "hoja_shared_types.h"

#include "utilities/crosscore_snapshot.h"
#include <stdint.h>

void sysmon_task(uint64_t timestamp);

#endif