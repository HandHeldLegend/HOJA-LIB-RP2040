/**
 * @file stick_scaling.c
 * @brief This file contains the implementation of stick scaling functions.
 */

#include "input/stick_scaling.h"
#include "hal/mutex_hal.h"
#include "utilities/settings.h"

#include "settings_shared_types.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TOTAL_ANGLES 64
#define ADJUSTABLE_ANGLES 16
#define DEFAULT_ANGLES_COUNT 8
#define ANGLE_MAP_COUNT ADJUSTABLE_ANGLES
#define ANGLE_CHUNK (float) 5.625f
#define HALF_ANGLE_CHUNK (float) ANGLE_CHUNK/2
#define ANGLE_MAPPING_CHUNK (float) 22.5f 
#define ANGLE_DEFAULT_CHUNK (float) (360.0f / 8.0f)
#define ANALOG_MAX_DISTANCE 2048
#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))
#define NORMALIZE(value, min, max) (((value) - (min)) / ((max) - (min)))
#define MAX_ANGLE_ADJUSTMENT 10
#define MINIMUM_REQUIRED_DISTANCE 1000

typedef struct 
{
  float distance_a;
  float distance_mid;
  float distance_b;
} polygon_solved_s;

typedef struct 
{
  int max_idx; 
  angleMap_s angle_maps[ADJUSTABLE_ANGLES];
  float polygon_mid_distances[ADJUSTABLE_ANGLES];
  int round_distances[64];
  analog_scaler_t scaling_mode;
} angle_setup_s;

#define ANGLE_SETUP_DEFAULT (angle_setup_s) {.angle_maps = {0}, \
                                             .max_idx = ADJUSTABLE_ANGLES-1, \
                                             .round_distances = {0}, \
                                             .scaling_mode = ANALOG_SCALER_ROUND}

angle_setup_s _left_setup   = ANGLE_SETUP_DEFAULT;
angle_setup_s _right_setup  = ANGLE_SETUP_DEFAULT;

bool _sticks_calibrating = false;

MUTEX_HAL_INIT(_stick_scaling_mutex);

float _normalize_angle(float angle);
float _angle_diff(float angle1, float angle2);
int   _find_lower_index(float input_angle, angleMap_s *map);
bool  _set_angle_mapping(int index, float input_angle, float output_angle, angleMap_s *map);
float _transform_angle(float input_angle, angleMap_s *map);

// Reset distances 
void _zero_distances(angle_setup_s *setup)
{
  for(int i = 0; i < TOTAL_ANGLES; i++)
  {
    setup->round_distances[i] = 5;
  }
}

// Initialize angle setup to defaults
void _angle_setup_reset_to_defaults(angle_setup_s *setup)
{
  setup->max_idx = DEFAULT_ANGLES_COUNT - 1;
  setup->scaling_mode = 0;

  // Reset to zero
  memset(setup->angle_maps, 0, ANGLE_MAP_SIZE * ADJUSTABLE_ANGLES);

  // Set up angle maps
  for(int i = 0; i < DEFAULT_ANGLES_COUNT; i++)
  {
    setup->angle_maps[i].input  = i * ANGLE_DEFAULT_CHUNK;
    setup->angle_maps[i].output = i * ANGLE_DEFAULT_CHUNK;
    setup->angle_maps[i].distance = ANALOG_MAX_DISTANCE;
  }

  // Set up distances 
  for(int i = 0; i < 64; i++)
  {
    setup->round_distances[i] = ANALOG_MAX_DISTANCE;
  }

  setup->scaling_mode = ANALOG_SCALER_ROUND;
}


// Comparison function for qsort
int _compare_by_input(const void *a, const void *b) {
    const angleMap_s *angle_a = (const angleMap_s *)a;
    const angleMap_s *angle_b = (const angleMap_s *)b;

    if (angle_a->input < angle_b->input) {
        return -1;
    } else if (angle_a->input > angle_b->input) {
        return 1;
    } else {
        return 0;
    }
}

// Get distance to coordinates based on a 0, 0 center
float _coordinate_distance(int x, int y) {
    // Use Pythagorean theorem to calculate distance
    // Convert to float to ensure floating-point precision
    return sqrtf((float)(x * x + y * y));
}

// Calculates the shortest angular distance between two angles
float _angle_diff(float angle1, float angle2)
{
  float diff = fmodf(fabsf(angle1 - angle2), 360.0f); // Normalize difference to 0–360
  return diff > 180.0f ? 360.0f - diff : diff;        // Use the shorter path
}

// Set mapping with angle difference validation
bool  _set_angle_mapping(int index, float input_angle, float output_angle, angleMap_s *map)
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
void _find_containing_index_pair(float input_angle, angle_setup_s *setup, int *idx_pair)
{
  idx_pair[0] = -1;
  idx_pair[1] = -1;

  if(setup->angle_maps[0].input > 0)
  {
    if(input_angle < setup->angle_maps[0].input)
    {
      idx_pair[0] = (setup->max_idx);
      idx_pair[1] = 0;
      return;
    }
  }

  if( (input_angle >= setup->angle_maps[(setup->max_idx)].input) )
  {
    idx_pair[0] = (setup->max_idx);
    idx_pair[1] = 0;
    return;
  }

  for (int i = 0; i < (setup->max_idx); i++)
  {
    if( (input_angle >= setup->angle_maps[i].input) && (input_angle < setup->angle_maps[i+1].input) )
    {
      idx_pair[0] = i;
      idx_pair[1] = i+1;
      return;
    }
  }
}

float _angle_magnitude(float input, float lower, float upper)
{ 
  if(upper < lower)
  {
    upper += 360.0f;
    if(input < lower) input += 360.0f;
  }

  float distance = _angle_diff(lower, upper);
  float magnitude_distance = _angle_diff(lower, input);

  return (magnitude_distance/distance);
}

float _angle_lerp(float magnitude, float lower, float upper)
{
  if(upper < lower)
  {
    upper += 360.0f;
  }

  float distance = _angle_diff(lower, upper);
  float new_out = (distance*magnitude) + lower;
  return _normalize_angle(new_out);
}

float _distance_lerp(float magnitude, float lower, float upper) 
{
  float distance = upper-lower;
  float new_out = lower+magnitude * (upper-lower);
  return new_out;
}

// Transform input angle based on configured mappings
float _transform_angle(float input_angle, angleMap_s *map)
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

void _angle_distance_to_fcoordinate(float angle, float distance, float *out) 
{    
    // Normalize angle to 0-360 range
    angle = fmodf(angle, 360.0f);
    if (angle < 0) angle += 360.0f;
    
    // Convert angle to radians
    float angle_radians = angle * M_PI / 180.0f;
    
    // Calculate X and Y coordinates
    // Limit to -2048 to +2048 range
    out[0] = (distance * cosf(angle_radians));
    out[1] = (distance * sinf(angle_radians));
    
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

void _calibrate_axis(int *in, angle_setup_s *setup)
{
  // Get input distance
  float distance  = _coordinate_distance(in[0], in[1]);
  // Get input angle
  float angle     = _coordinate_to_angle(in[0], in[1]);

  angle   -= HALF_ANGLE_CHUNK;
  angle   = roundf(angle/ANGLE_CHUNK);
  int idx = (int) angle % 64;

  // Assign distance if needed
  if(distance > setup->round_distances[idx])
  {
    setup->round_distances[idx] = distance;
  }
}

float _closest_distance_to_line(float x1, float y1, float x2, float y2) {
    // Calculate the line equation coefficients (Ax + By + C = 0)
    float A = y2 - y1;
    float B = x1 - x2;
    float C = x2 * y1 - x1 * y2;

    // Calculate the distance from (0, 0) to the line
    float distance = fabs(A * 0 + B * 0 + C) / sqrt(A * A + B * B);

    return distance;
}

void _solve_polygon(float d1, float a1, float d2, float a2, float *mid_distance)
{
  float c1[2] = {0};
  float c2[2] = {0};

  _angle_distance_to_fcoordinate(a1, d1, c1);
  _angle_distance_to_fcoordinate(a2, d2, c2);

  *mid_distance = _closest_distance_to_line(c1[0], c1[1], c2[0], c2[1]);
}

void _process_axis(int *in, int *out, angle_setup_s *setup)
{
  // Get input distance
  float distance = _coordinate_distance(in[0], in[1]);

  // Get input angle
  float angle = _coordinate_to_angle(in[0], in[1]);

  // Get the index pair from our calibrated angles
  // (Which indeces is our angle between?)
  int idx_pair[2] = {-1,-1};
  _find_containing_index_pair(angle, setup, idx_pair);
  
  // Only process if we have a valid pair
  if( (idx_pair[0]>-1) && (idx_pair[1]>-1))
  {
    // Calculate magnitude of current angle between input angles for our angle map
    float magnitude = _angle_magnitude(angle, setup->angle_maps[idx_pair[0]].input, setup->angle_maps[idx_pair[1]].input);
    // Calculate new output angle based on magnitudes
    float new_angle = _angle_lerp(magnitude, setup->angle_maps[idx_pair[0]].output, setup->angle_maps[idx_pair[1]].output);

    // Variable initializations
    float angle_floor = 0;
    int   distance_index_lower  = 0;
    int   distance_index_upper  = 0;
    float distance_magnitude    = 0;
    float target_distance       = ANALOG_MAX_DISTANCE;
    float calibrated_distance   = 0;
    float distance_scale_factor = 1;
    float output_distance       = 0;

    // Determine which polygon half we are in 
    int polygon_half = (magnitude >= 0.5) ? 1 : 0;
    float magnitude_expanded = (magnitude >= 0.5) ? (magnitude-0.5)*2 : magnitude*2;

    // We will calculate the new distance appropriately according to our scaling mode
    switch(setup->scaling_mode)
    {
      case ANALOG_SCALER_POLYGON:

        if(polygon_half) // Latter half
        {
          target_distance       = _distance_lerp(magnitude_expanded, 
          setup->polygon_mid_distances[idx_pair[0]],
          setup->angle_maps[idx_pair[1]].distance);
        }
        else // First half
        {
          target_distance       = _distance_lerp(magnitude_expanded, 
          setup->angle_maps[idx_pair[0]].distance, 
          setup->polygon_mid_distances[idx_pair[0]]);
        }

      default:
      case ANALOG_SCALER_ROUND:
        // Get our 64 angle index
        if(angle >= (ANGLE_CHUNK*63))
        {
          distance_index_lower = 63;
          distance_index_upper = 0;
        }
        else 
        {
          distance_index_lower  = floorf(angle / ANGLE_CHUNK);
          distance_index_upper  = (distance_index_lower+1);
        }
        
        angle_floor           = distance_index_lower*ANGLE_CHUNK;
        
        // Calculate the magnitude of our angle for the 64 chunks
        distance_magnitude    = _angle_magnitude(angle, angle_floor, angle_floor+ANGLE_CHUNK);

        // Calculate a calibrated distance that is interpolated from our saved actual distances
        calibrated_distance   = _distance_lerp(distance_magnitude, 
          setup->round_distances[distance_index_lower], 
          setup->round_distances[distance_index_upper]);        
      break;

    }

    float adjusted_distance = target_distance + 125;

    distance_scale_factor = (!calibrated_distance) ? 0 : (adjusted_distance / calibrated_distance);
    output_distance = distance*distance_scale_factor;

    if(output_distance > target_distance)
    {
      output_distance = target_distance;
    }

    _angle_distance_to_coordinate(new_angle, output_distance, out);
  }
}

void _unpack_stick_distances(analogPackedDistances_s *distances, angle_setup_s *setup)
{
  // Set first distance
  setup->round_distances[0] = distances->first_distance;
   
  for(int i = 1; i < TOTAL_ANGLES; i++)
  {
    setup->round_distances[i] = setup->round_distances[i-1] + (int) distances->offsets[i-1];
  }
}

void _pack_stick_distances(analogPackedDistances_s *distances, angle_setup_s *setup)
{
  // Set first distance
  distances->first_distance = setup->round_distances[0];
   
  for(int i = 1; i<TOTAL_ANGLES; i++)
  {
    int16_t diff = (int16_t) setup->round_distances[i] - (int16_t) setup->round_distances[i-1];
    distances->offsets[i-1] = diff;
  }
}

// Write distance data to config block
void _write_to_config_block()
{
  analogPackedDistances_s packed = {0};

  _pack_stick_distances(&packed, &_left_setup);
  memcpy(&(analog_config->l_packed_distances), &packed, ANALOG_PACKED_DISTANCES_SIZE);

  _pack_stick_distances(&packed, &_right_setup);
  memcpy(&(analog_config->r_packed_distances), &packed, ANALOG_PACKED_DISTANCES_SIZE);

  for(int i = 0; i < ADJUSTABLE_ANGLES; i++)
  {
    analog_config->l_angle_maps[i].distance = _left_setup.angle_maps[i].distance;
    analog_config->l_angle_maps[i].input    = _left_setup.angle_maps[i].input;
    analog_config->l_angle_maps[i].output   = _left_setup.angle_maps[i].output;

    analog_config->r_angle_maps[i].distance = _right_setup.angle_maps[i].distance;
    analog_config->r_angle_maps[i].input    = _right_setup.angle_maps[i].input;
    analog_config->r_angle_maps[i].output   = _right_setup.angle_maps[i].output;
  }
}

// Read parameters from config block
void _read_from_config_block() 
{
  memcpy(_left_setup.angle_maps,   analog_config->l_angle_maps, ANGLE_MAP_SIZE * ANGLE_MAP_COUNT);
  memcpy(_right_setup.angle_maps,  analog_config->r_angle_maps, ANGLE_MAP_SIZE * ANGLE_MAP_COUNT);

  analogPackedDistances_s tmp = {0};
  memcpy(&tmp, &(analog_config->l_packed_distances), ANALOG_PACKED_DISTANCES_SIZE);
  _unpack_stick_distances(&tmp, &_left_setup);
  memcpy(&tmp, &(analog_config->r_packed_distances), ANALOG_PACKED_DISTANCES_SIZE);
  _unpack_stick_distances(&tmp, &_right_setup);

  _left_setup.scaling_mode  = analog_config->l_scaler_type;
  _right_setup.scaling_mode = analog_config->r_scaler_type;
}

// Validates a setup
void _validate_setup(angle_setup_s *setup)
{
  angleMap_s filtered_map[ADJUSTABLE_ANGLES] = {0};
  int used_size_max = 0;

  for(int i = 0; i < ADJUSTABLE_ANGLES; i++)
  {
    if(setup->angle_maps[i].distance<=MINIMUM_REQUIRED_DISTANCE)
    {
      continue;
    }

    filtered_map[used_size_max].distance = setup->angle_maps[i].distance;
    filtered_map[used_size_max].input    = setup->angle_maps[i].input;
    filtered_map[used_size_max].output   = setup->angle_maps[i].output;
    used_size_max++;
  }

  // Sort our angles by input
  qsort(filtered_map, used_size_max, ANGLE_MAP_SIZE, _compare_by_input);

  memset(setup->angle_maps, 0, ANGLE_MAP_SIZE * ADJUSTABLE_ANGLES);
  memcpy(setup->angle_maps, filtered_map, ANGLE_MAP_SIZE * ADJUSTABLE_ANGLES);

  // Set up polygon solves
  for(int i = 0; i < used_size_max; i++) {
    _solve_polygon(setup->angle_maps[i].distance, setup->angle_maps[i].output, 
                   setup->angle_maps[(i+1)%used_size_max].distance, setup->angle_maps[(i+1)%used_size_max].output, 
                   &setup->polygon_mid_distances[i]);
  }

  if(used_size_max < 4) // We need 4 valid angles
  {
    _angle_setup_reset_to_defaults(setup);
  }
  else 
  {
    setup->max_idx = used_size_max-1;
  }
}

void stick_scaling_calibrate_start(bool start)
{
  MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);
  if(!_sticks_calibrating && start) 
  {
    // Reset all to default
    _zero_distances(&_left_setup);
    _zero_distances(&_right_setup);

    _sticks_calibrating = true;
  }
  else if (_sticks_calibrating && !start) 
  {
    _write_to_config_block();
    _sticks_calibrating = false;
  }
  MUTEX_HAL_EXIT(&_stick_scaling_mutex);
}

void stick_scaling_default_check()
{
  if(analog_config->analog_config_version != CFG_BLOCK_ANALOG_VERSION)
  {
    analog_config->analog_config_version = CFG_BLOCK_ANALOG_VERSION;

    // Set to default left setup
    _angle_setup_reset_to_defaults(&_left_setup);
    _angle_setup_reset_to_defaults(&_right_setup);

    _write_to_config_block();
  }
}

// Input and output are based around -2048 to 2048 values with 0 as centers
void stick_scaling_process(analog_data_s *in, analog_data_s *out)
{
  int in_left[2]    = {in->lx, in->ly};
  int in_right[2]   = {in->rx, in->ry};

  int out_left[2];
  int out_right[2];

  MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);
  if(_sticks_calibrating)
  {
    _calibrate_axis(in_left, &_left_setup);
    _calibrate_axis(in_right, &_right_setup);
    out_left[0] = 0;
    out_left[1] = 0;
    out_right[0] = 0;
    out_right[1] = 0;
  }
  else 
  {
    _process_axis(in_left,  out_left,   &_left_setup);
    _process_axis(in_right, out_right,  &_right_setup);
  }
  MUTEX_HAL_EXIT(&_stick_scaling_mutex);

  out->lx = out_left[0];
  out->ly = out_left[1];
  out->rx = out_right[0];
  out->ry = out_right[1];
}

bool stick_scaling_init()
{
  MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);

  // Load from config
  _read_from_config_block();

  // Perform a default check. This applies defaults
  // if the config block version is a mismatch
  stick_scaling_default_check();

  // Validate our settings
  _validate_setup(&_left_setup); 
  _validate_setup(&_right_setup); 

  MUTEX_HAL_EXIT(&_stick_scaling_mutex);
  return true;
}
