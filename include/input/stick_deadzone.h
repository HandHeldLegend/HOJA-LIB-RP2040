#ifndef INPUT_STICK_DEADZONE_H
#define INPUT_STICK_DEADZONE_H

#include "input_shared_types.h"

void stick_deadzone_process(analog_data_s *in, analog_data_s *out);

#endif