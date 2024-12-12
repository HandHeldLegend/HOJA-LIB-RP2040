#ifndef INPUT_STICK_SCALING_H
#define INPUT_STICK_SCALING_H

#include <stdint.h>
#include <stdbool.h>
#include "input_shared_types.h"

void stick_scaling_calibrate_start(bool start);
void stick_scaling_process(analog_data_s *in, analog_data_s *out);
bool stick_scaling_init();

#endif
