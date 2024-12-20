#ifndef INPUT_STICK_SCALING_H
#define INPUT_STICK_SCALING_H

#include <stdint.h>
#include <stdbool.h>
#include "input_shared_types.h"

float stick_scaling_coordinates_to_angle(int x, int y);
void stick_scaling_calibrate_start(bool start);
void stick_scaling_process(analog_data_s *in, analog_data_s *out);
bool stick_scaling_init();

#endif
