#ifndef SNAPBACKUTILS_H 
#define SNAPBACKUTILS_H

#include <stdint.h>
#include <stdbool.h>
#include "input_shared_types.h"

void sbutil_axis_to_data(analog_axis_s *l, analog_axis_s *r, analog_data_s *data);
void sbutil_data_to_axis(analog_data_s *data, analog_axis_s *l, analog_axis_s *r);
int sbutil_is_opposite_direction(int x1, int y1, int x2, int y2);
float sbutil_get_2d_distance(int x, int y);
bool sbutil_is_distance_falling(float last_distance, float current_distance);
bool sbutil_is_between(int centerValue, int lastPosition, int currentPosition);
int sbutil_get_distance(int A, int B);
int8_t sbutil_get_direction(int currentPosition, int lastPosition);

#endif