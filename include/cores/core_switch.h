#ifndef CORES_SWITCH_H
#define CORES_SWITCH_H

#include "cores/cores.h"
#include "input_shared_types.h"

bool core_switch_init(core_params_s *params);

// Hand the Switch motion gesture inputs (flicks) to the IMU. The Switch core does this itself;
// transports that build their own Switch reports (ESP32 baseband) call it with their input.
void core_switch_gesture_input(const mapper_input_s *input);

#endif