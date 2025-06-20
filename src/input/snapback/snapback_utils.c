#include "input/snapback/snapback_utils.h"
#include <math.h>   
#include <stdlib.h>
#include <string.h>

void sbutil_axis_to_data(analog_axis_s *l, analog_axis_s *r, analog_data_s *data)
{
    // Left axis
    data->lx = l->x;
    data->ly = l->y;
    data->langle = l->angle;
    data->ldistance = l->distance;
    data->ltarget = l->target;

    // Right axis
    data->rx = r->x;
    data->ry = r->y;
    data->rangle = r->angle;
    data->rdistance = r->distance;
    data->rtarget = r->target;
}

void sbutil_data_to_axis(analog_data_s *data, analog_axis_s *l, analog_axis_s *r)
{
    // Left axis
    l->x = data->lx;
    l->y = data->ly;
    l->angle = data->langle;
    l->distance = data->ldistance;
    l->target = data->ltarget;
    l->axis_idx = 0; // Left axis index

    // Right axis
    r->x = data->rx;
    r->y = data->ry;
    r->angle = data->rangle;
    r->distance = data->rdistance;
    r->target = data->rtarget;
    r->axis_idx = 1; // Right axis index
}

int sbutil_is_opposite_direction(int x1, int y1, int x2, int y2)
{
    // Opposite directions if dot product is negative.
    return (x1) * (x2) + (y1) * (y2) < 0;
}

float sbutil_get_2d_distance(int x, int y)
{
  float dx = (float)x;
  float dy = (float)y;
  return sqrtf(dx * dx + dy * dy);
}

bool sbutil_is_distance_falling(float last_distance, float current_distance)
{
    return (current_distance <= last_distance);
}

bool sbutil_is_between(int centerValue, int lastPosition, int currentPosition) 
{
    // Check if the current position and last position are on different sides of the center
    if ((currentPosition >= centerValue && lastPosition < centerValue) ||
        (currentPosition < centerValue && lastPosition >= centerValue)) {
        return true;  // Sensor has crossed the center point
    } else {
        return false; // Sensor has not crossed the center point
    }
}

int sbutil_get_distance(int A, int B)
{
    return abs(A-B);
}

int8_t sbutil_get_direction(int currentPosition, int lastPosition)
{
    if(currentPosition==lastPosition) return 0;

    if(currentPosition > lastPosition) return 1;
    else return -1;
}