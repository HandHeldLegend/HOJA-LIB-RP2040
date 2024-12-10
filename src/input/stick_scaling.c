/**
 * @file stick_scaling.c
 * @brief This file contains the implementation of stick scaling functions.
 */

#include "input/stick_scaling.h"
#include "input_shared_types.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TOTAL_ANGLES 64
#define ADJUSTABLE_ANGLES 16
#define ANGLE_CHUNK (float) 5.625f
#define ANGLE_MAPPING_CHUNK (float) 22.5f;
#define ANALOG_MAX_DISTANCE 2048
#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))
#define NORMALIZE(value, min, max) (((value) - (min)) / ((max) - (min)))
#define MAX_ANGLE_ADJUSTMENT 10
#define MINIMUM_REQUIRED_DISTANCE 1000

typedef struct
{
  float distance;
  float angle;
} angle_data_s;

typedef struct
{
  float input;    // Input Angle
  float output;   // Output Angle
  float distance; // Distance to this input angle maximum for hard polygonal shape stick gates
} angle_map_s;

int _angle_map_l_max_idx = ADJUSTABLE_ANGLES-1;
int _angle_map_r_max_idx = ADJUSTABLE_ANGLES-1;
angle_map_s _angle_map_l[ADJUSTABLE_ANGLES] = {0};
angle_map_s _angle_map_r[ADJUSTABLE_ANGLES] = {0};

// Index represents actual measured angle from raw sensor data
// Results in great calibration points for round output
float _distances_l[64] = {0};
float _distances_r[64] = {0};

analog_scaler_t _scaling_mode = ANALOG_SCALER_ROUND;

float _normalize_angle(float angle);
float _angle_diff(float angle1, float angle2);
int   _find_lower_index(float input_angle, angle_map_s *map);
bool  _set_angle_mapping(int index, float input_angle, float output_angle, angle_map_s *map);
float _transform_angle(float input_angle, angle_map_s *map);

// Comparison function for qsort
int _compare_by_input(const void *a, const void *b) {
    const angle_map_s *angle_a = (const angle_map_s *)a;
    const angle_map_s *angle_b = (const angle_map_s *)b;

    if (angle_a->input < angle_b->input) {
        return -1;
    } else if (angle_a->input > angle_b->input) {
        return 1;
    } else {
        return 0;
    }
}

void _set_default_distances()
{
  for(int i = 0; i < 64; i++)
  {
    _distances_l[i] = ANALOG_MAX_DISTANCE;
    _distances_r[i] = ANALOG_MAX_DISTANCE;
  }
}

void _set_default_angle_mappings()
{
  for(int i = 0; i < ADJUSTABLE_ANGLES; i++)
  {
    _angle_map_l[i].input  = i*ANGLE_MAPPING_CHUNK;
    _angle_map_l[i].output = i*ANGLE_MAPPING_CHUNK;
    _angle_map_l[i].distance = ANALOG_MAX_DISTANCE;

    _angle_map_r[i].input  = i*ANGLE_MAPPING_CHUNK;
    _angle_map_r[i].output = i*ANGLE_MAPPING_CHUNK;
    _angle_map_r[i].distance = ANALOG_MAX_DISTANCE;
  }
}

// Function to sort the array
int _validate_angle_map(angle_map_s *in) {

  int unused_idx_max = ADJUSTABLE_ANGLES; 
  int used_idx_max = 0;
  angle_map_s filtered_map[ADJUSTABLE_ANGLES] = {0};

  // First, remove all unused angles and put them at the end
  for(int i = 0; i < ADJUSTABLE_ANGLES; i++)
  {
    if(in[i].distance<MINIMUM_REQUIRED_DISTANCE)
    {
      unused_idx_max--;
    }
    else
    {
      filtered_map[used_idx_max] = in[i];
      used_idx_max++;
    }
  }

  qsort(filtered_map, used_idx_max, sizeof(angle_map_s), _compare_by_input);

  float tmp_input   = filtered_map[0].input;
  float tmp_output  = filtered_map[0].output;

  if(used_idx_max<4) return -4;

  // Check for duplicates or invalid mappings
  for(int i = 1; i < used_idx_max; i++)
  {
    if(in[i].input == tmp_input) return -1;
    if(_angle_diff(tmp_input, tmp_output) >= MAX_ANGLE_ADJUSTMENT) return -2;
    if(_angle_diff(in[i].output, tmp_output) >= MAX_ANGLE_ADJUSTMENT) return -3;

    tmp_input = in[i].input;
    tmp_output = in[i].output;
  }

  return used_idx_max;
}

float _coordinate_distance(int x, int y) {
    // Use Pythagorean theorem to calculate distance
    // Convert to float to ensure floating-point precision
    return sqrtf((float)(x * x + y * y));
}

// This function takes in the original angle/distance pair
// along with the distances saved from calibration. It returns
// the linear scaled distance
float _get_scaled_distance(int *in_coords, float angle, float *distances)
{
  float in_distance = _coordinate_distance(in_coords[0], in_coords[1]);

  // Floor our angle to get that
  int idx_1 = (int)(angle / ANGLE_CHUNK);
  float angle_1 = ANGLE_CHUNK * (float)idx_1;
  float angle_2 = angle_1 + ANGLE_CHUNK;

  // Figure out which indexes
  // and figure out the percent along we are
  float fractional = NORMALIZE(angle, angle_1, angle_2);

  // Get next idx. Loop around 64.
  int idx_2 = (idx_1 + 1) % 64;

  // Calculate distance
  float distance = LERP(distances[idx_1], distances[idx_2], fractional);
  float d_fractional = NORMALIZE(in_distance, 0, distance);

  float out_distance = ANALOG_MAX_DISTANCE * d_fractional;

  return out_distance;
}

// Calculates the shortest angular distance between two angles
float _angle_diff(float angle1, float angle2)
{
  float diff = fmodf(fabsf(angle1 - angle2), 360.0f); // Normalize difference to 0–360
  return diff > 180.0f ? 360.0f - diff : diff;        // Use the shorter path
}

// Set mapping with angle difference validation
bool  _set_angle_mapping(int index, float input_angle, float output_angle, angle_map_s *map)
{
  // Validate index
  if (index < 0 || index >= ADJUSTABLE_ANGLES)
  {
    return false;
  }

  // Normalize input and output angles
  input_angle   = _normalize_angle(input_angle);
  output_angle  = _normalize_angle(output_angle);

  // If this is the first point, always allow
  if (index == 0)
  {
    map[index].input = input_angle;
    map[index].output = output_angle;
    return true;
  }

  // Check previous point
  int prev_index = (index - 1 + ADJUSTABLE_ANGLES) % ADJUSTABLE_ANGLES;
  float prev_input  = map[prev_index].input;
  float prev_output = map[prev_index].output;

  // Calculate input and output angle differences
  float input_distance  = _angle_diff(input_angle, prev_input);
  float output_distance = _angle_diff(output_angle, prev_output);

  // Validate that both input and output differences are within ±10 degrees
  if (input_distance >= MAX_ANGLE_ADJUSTMENT || output_distance >= MAX_ANGLE_ADJUSTMENT)
  {
    return false;
  }

  // Store the mapping
  map[index].input  = input_angle;
  map[index].output = output_angle;

  return true;
}

// Normalize angle to 0-360 range
float _normalize_angle(float angle)
{
  angle = fmodf(angle, 360.0f);
  return angle < 0 ? angle + 360.0f : angle;
}

// Find the low and high index pair that contains our angle
void _find_containing_index_pair(float input_angle, angle_map_s *map, int *idx_pair)
{
  idx_pair[0] = -1;
  idx_pair[1] = -1;

  for (int i = 0; i < (ADJUSTABLE_ANGLES-1); i++)
  {
    if( (input_angle >= map[i].input) && (input_angle < map[i+1].input) )
    {
      idx_pair[0] = i;
      idx_pair[1] = i+1;
      return;
    }
  }

  if( (input_angle >= map[ADJUSTABLE_ANGLES-1].input) || (input_angle < map[0].input) )
  {
    idx_pair[0] = ADJUSTABLE_ANGLES-1;
    idx_pair[1] = 0;
    return;
  }
}

// Transform input angle based on configured mappings
float _transform_angle(float input_angle, angle_map_s *map)
{
  // Normalize input angle
  input_angle = _normalize_angle(input_angle);

  // Find the lower and upper mapping indices
  int lower_index = _find_lower_index(input_angle, map);
  int upper_index = (lower_index + 1) % ADJUSTABLE_ANGLES;

  // Get input and output angles for lower and upper points
  float lower_input   = map[lower_index].input;
  float lower_output  = map[lower_index].output;
  float upper_input   = map[upper_index].input;
  float upper_output  = map[upper_index].output;

  // Handle wrap-around for input angles
  if (upper_input < lower_input)
  {
    upper_input += 360.0f;
    if (input_angle < lower_input)
      input_angle += 360.0f;
  }

  // Linear interpolation
  float t = (input_angle - lower_input) / (upper_input - lower_input);
  float output_angle = lower_output + t * (upper_output - lower_output);

  // Normalize output angle
  return _normalize_angle(output_angle);
}

void _angle_distance_to_coordinate(float angle, float distance, int *out) 
{    
    // Normalize angle to 0-360 range
    angle = fmodf(angle, 360.0f);
    if (angle < 0) angle += 360.0f;
    
    // Convert angle to radians
    float angle_radians = angle * M_PI / 180.0f;
    
    // Calculate X and Y coordinates
    // Limit to -2048 to +2048 range
    out[0] = (int)(distance * cosf(angle_radians));
    out[1] = (int)(distance * sinf(angle_radians));
    
    // Clamp to prevent exceeding the specified range
    out[0] = fmaxf(-2048, fminf(2048, out[0]));
    out[1] = fmaxf(-2048, fminf(2048, out[1]));
}

float _coordinate_to_angle(int x, int y) 
{
    // Handle special cases to avoid division by zero
    if (x == 0) {
        // Vertical lines
        return (y > 0) ? 90.0f : 270.0f;
    }
    
    // Calculate arctangent and convert to degrees
    float angle_radians = atan2f((float)y, (float)x);
    
    // Convert to degrees and normalize to 0-360 range
    float angle_degrees = angle_radians * 180.0f / M_PI;
    
    // Adjust to ensure 0-360 range with positive angles
    if (angle_degrees < 0) {
        angle_degrees += 360.0f;
    }
    
    return angle_degrees;
}

void _process_axis(int *in, int *out, angle_map_s *map, float *distances)
{
  float distance = 0;
  float angle = _coordinate_to_angle(in[0], in[1]);

  int idx_pair[2] = {-1,-1};
  _find_containing_index_pair(angle, map, idx_pair);
  

  if( (idx_pair[0]>-1) && (idx_pair[1]>-1))
  {
    float out_angle = _transform_angle(angle, map);

    switch(_scaling_mode)
    {
      default:
      case ANALOG_SCALER_ROUND:
      break;

      case ANALOG_SCALER_POLYGON:
      break;
    }


    distance  = _get_scaled_distance(in, angle, distances);
    _angle_distance_to_coordinate(out_angle, distance, out);
  }

  
}

bool stick_scaling_init()
{
  _set_default_distances();
  _set_default_angle_mappings();
}

void stick_scaling_process(analog_data_s *in, analog_data_s *out)
{
  int in_left[2]    = {in->lx-2048, in->ly-2048};
  int in_right[2]   = {in->rx-2048, in->ry-2048};

  int out_left[2];
  int out_right[2];

  _process_axis(in_left,  out_left,   _angle_map_l, _distances_l);
  _process_axis(in_right, out_right,  _angle_map_r, _distances_r);

  out->lx = out_left[0];
  out->ly = out_left[1];
  out->rx = out_right[0];
  out->ry = out_right[1];
}