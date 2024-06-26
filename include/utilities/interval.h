#ifndef INTERVAL_H
#define INTERVAL_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool interval_run(uint32_t timestamp, uint32_t interval, interval_s *state);
bool interval_resettable_run(uint32_t timestamp, uint32_t interval, bool reset, interval_s *state);

#ifdef __cplusplus
}
#endif

#endif