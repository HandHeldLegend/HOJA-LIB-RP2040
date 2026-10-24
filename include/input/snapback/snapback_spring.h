#ifndef INPUT_SNAPBACK_SPRING_H
#define INPUT_SNAPBACK_SPRING_H

#include <stdint.h>
#include "input_shared_types.h"

// Snapback type 3: holds a released stick's rebound at neutral and passes everything the thumb does.
// A stick left to itself rings like a damped spring; each sample is compared with where that spring
// would be, so a move the spring can't explain (the thumb) goes through as it is.
void snapback_spring_process(analog_axis_s *input, analog_axis_s *output);
// Takes each stick's tuning from the analog settings (built-in defaults when none is stored).
// Call after the analog settings load or change.
void snapback_spring_load(void);

#endif
