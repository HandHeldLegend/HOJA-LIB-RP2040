#ifndef INTERVAL_H
#define INTERVAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
  uint32_t this_time;
  uint32_t last_time;
} interval_s;

bool interval_run(uint32_t timestamp, uint32_t interval, interval_s *state);
bool interval_resettable_run(uint32_t timestamp, uint32_t interval, bool reset, interval_s *state);

#ifdef __cplusplus
}
#endif

#endif