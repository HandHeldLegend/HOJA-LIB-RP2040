#ifndef INPUT_SNAPBACK_CAPTURE_H
#define INPUT_SNAPBACK_CAPTURE_H

#include <stdint.h>
#include "input_shared_types.h"

// Records a stick as it comes back from a push (a release, a turnaround, a flick), for tuning the
// snapback filter: both axes at the full analog rate, as the filter sees them, sent over WebUSB

#define SNAPBACK_CAPTURE_OFF 0xFF

// stick 0 (left) or 1 (right) arms the capture, SNAPBACK_CAPTURE_OFF stops it
void snapback_capture_arm(uint8_t stick);
// Every analog sample, before the filter
void snapback_capture_process(const analog_axis_s *l, const analog_axis_s *r);

#endif
