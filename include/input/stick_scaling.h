#ifndef STICK_H
#define STICK_H

#include "hoja_includes.h"

void stick_scaling_get_settings();
void stick_scaling_set_settings();

void stick_scaling_init();

void stick_scaling_reset_distances();

float stick_get_angle(int x, int y, int center_x, int center_y);
void stick_normalized_vector(float angle, float *x, float *y);
float stick_get_distance(int x, int y, int center_x, int center_y);
void stick_scaling_get_octant_axis_offset(a_data_s *in, uint8_t *axis, uint8_t *octant);
void stick_scaling_get_octant_axis(a_data_s *in, uint8_t *axis, uint8_t *octant);
bool stick_scaling_capture_distances(a_data_s *in);
void stick_scaling_capture_center(a_data_s *in);
bool stick_scaling_capture_angle(a_data_s *in);
void stick_scaling_process_data(a_data_s *in, a_data_s *out);
void stick_apply_deadzone(a_data_s *data);

#endif
