#ifndef HOJA_STICK_SCALING_H
#define HOJA_STICK_SCALING_H

#include <stdint.h>
#include "input/analog.h"

void stick_scaling_get_settings();
void stick_scaling_set_settings();

void stick_scaling_init();

void stick_scaling_reset_distances();

float stick_get_angle(int x, int y, int center_x, int center_y);
void stick_normalized_vector(float angle, float *x, float *y);
float stick_get_distance(int x, int y, int center_x, int center_y);
void stick_scaling_get_octant_axis_offset(analog_data_s *in, uint8_t *axis, uint8_t *octant);
void stick_scaling_get_octant_axis(analog_data_s *in, uint8_t *axis, uint8_t *octant);
bool stick_scaling_capture_distances(analog_data_s *in);
void stick_scaling_capture_center(analog_data_s *in);
bool stick_scaling_capture_angle(analog_data_s *in);
void stick_scaling_process_data(analog_data_s *in, analog_data_s *out);
void stick_apply_deadzone(analog_data_s *data);

#endif
