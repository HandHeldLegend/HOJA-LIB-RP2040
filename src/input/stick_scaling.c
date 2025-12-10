/**
 * @file stick_scaling.c
 * @brief This file contains the implementation of stick scaling functions.
 */

#include "input/stick_scaling.h"
#include "input/analog.h"

#include "hoja.h"

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
#define ANGLE_CHUNK (float) 5.625f
#define HALF_ANGLE_CHUNK (float) ANGLE_CHUNK/2
#define ANGLE_MAPPING_CHUNK (float) 22.5f 
#define ANGLE_DEFAULT_CHUNK (float) (360.0f / 8.0f)
#define ANALOG_MAX_DISTANCE 2048
#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))
#define NORMALIZE(value, min, max) (((value) - (min)) / ((max) - (min)))
#define MAX_ANGLE_ADJUSTMENT 10
#define MINIMUM_REQUIRED_DISTANCE 1000

bool _sticks_calibrating = false;

MUTEX_HAL_INIT(_stick_scaling_mutex);

/**
 * @brief Normalizes an angle (in degrees) to the range [0.0, 360.0).
 * * @param angle The input angle, which can be positive or negative.
 * @return The normalized angle in the range [0.0, 360.0).
 */
float _angle_normalize(float angle)
{
    // 1. Calculate the remainder of the angle divided by 360.0.
    // This handles angles > 360 or angles < -360.
    // The result of fmodf(a, b) has the same sign as 'a'.
    float normalized = fmodf(angle, 360.0f);
    
    // 2. Adjust for negative results.
    // If the input was negative (e.g., -10.0), fmodf returns -10.0.
    // We add 360.0 to bring it into the [0, 360) range (e.g., -10.0 + 360.0 = 350.0).
    if (normalized < 0.0f)
    {
        normalized += 360.0f;
    }
    
    return normalized;
}

/**
 * @brief Finds the index of the closest in_angle that is enabled.
 * * @param in_angle The raw input angle.
 * @param config The array of calibration slots.
 * @return The index of the closest enabled slot, or -1 if none are enabled.
 */
int _joy_get_closest_enabled_index(float in_angle, const joyConfigSlot_s config[ADJUSTABLE_ANGLES])
{
    int closest_index = -1;
    float min_diff = 360.0f; // Max possible difference
    
    for (int i = 0; i < ADJUSTABLE_ANGLES; i++)
    {
        if (!config[i].enabled) continue;
        
        // Compute the shortest angular difference (e.g., diff between 355 and 5 is 10)
        // `fmodf(a - b + 180 + 360, 360) - 180` gives the signed shortest difference
        float diff = fabsf(fmodf(in_angle - config[i].in_angle + 180.0f + 360.0f, 360.0f) - 180.0f);
        
        if (diff < min_diff)
        {
            min_diff = diff;
            closest_index = i;
        }
    }
    
    return closest_index;
}

/**
 * @brief Finds the two closest enabled configuration indices that bracket the input angle.
 * * ASSUMPTION: The 'config' array has been partitioned and the enabled slots (0 to 'enabled_count - 1')
 * are sorted by their 'in_angle' in ascending order.
 * * @param in_angle The raw input angle.
 * @param config The array of calibration slots.
 * @param enabled_count The number of enabled, sorted slots in the front of the array.
 * @param index_a Pointer to store the index of the lower bracketing slot.
 * @param index_b Pointer to store the index of the upper bracketing slot.
 * @return true if two bracketing slots were found, false otherwise.
 */
bool _joy_find_bracketing_indices_fast(float in_angle, 
                                      const joyConfigSlot_s config[ADJUSTABLE_ANGLES], 
                                      int enabled_count, 
                                      int *index_a, 
                                      int *index_b)
{
    // If there are less than 2 enabled slots, interpolation is impossible.
    if (enabled_count < 2)
    {
        *index_a = -1;
        *index_b = -1;
        return false;
    }
    
    // Normalize the input angle for reliable comparison
    // NOTE: This assumes angle_normalize is available and used.
    float normalized_in_angle = _angle_normalize(in_angle); 

    // --- Linear Search for the Upper Bracketing Point (B) ---
    int i;
    for (i = 0; i < enabled_count; i++)
    {
        // Since the array is sorted, the first angle greater than the input is B.
        if (config[i].in_angle >= normalized_in_angle)
        {
            *index_b = i;
            break;
        }
    }
    
    if (i == enabled_count)
    {
        // Case 1: The input angle is greater than ALL enabled angles (e.g., in_angle=350, max angle=340).
        // This is the wrap-around case (360 -> 0).
        // B is the first element (index 0).
        // A is the last enabled element (index enabled_count - 1).
        *index_b = 0; 
        *index_a = enabled_count - 1;
    }
    else
    {
        // Case 2: Standard case. We found an angle >= normalized_in_angle at index i.
        *index_b = i;
        
        // A is the previous element. 
        // If i is 0, A is the last element (wrap-around case, e.g., in_angle=5, min angle=10).
        if (i == 0)
        {
            *index_a = enabled_count - 1;
        }
        else
        {
            *index_a = i - 1;
        }
    }
    
    return true;
}

// Used for internal sorting by in_angle
int _compare_joyConfigSlot_by_angle(const void *a, const void *b)
{
    const joyConfigSlot_s *slot_a = (const joyConfigSlot_s *)a;
    const joyConfigSlot_s *slot_b = (const joyConfigSlot_s *)b;
    
    if (slot_a->in_angle < slot_b->in_angle) return -1;
    if (slot_a->in_angle > slot_b->in_angle) return 1;
    return 0;
}

/**
 * @brief Sorts the array: enabled slots first (sorted by in_angle), then disabled slots.
 * * @param config The array of calibration slots (will be modified).
 * @return The count of enabled slots.
 */
int _joy_validation_sort_and_count(joyConfigSlot_s config[ADJUSTABLE_ANGLES])
{
    int enabled_count = 0;
    int write_ptr = 0;
    
    // --- Step 1: Partition Enabled and Disabled Slots ---
    // Use a two-pointer approach to move all enabled slots to the front.
    
    for (int read_ptr = 0; read_ptr < ADJUSTABLE_ANGLES; read_ptr++)
    {
        if (config[read_ptr].enabled)
        {
            // If the slot is enabled, check if a swap is needed
            if (read_ptr != write_ptr)
            {
                // Swap the enabled slot with the slot at the write pointer
                joyConfigSlot_s temp = config[write_ptr];
                config[write_ptr] = config[read_ptr];
                config[read_ptr] = temp;
            }
            write_ptr++;
            enabled_count++;
        }
    }
    
    // --- Step 2: Sort the Enabled Slots by in_angle ---
    // The enabled slots now occupy indices 0 to (enabled_count - 1).
    if (enabled_count > 1)
    {
        qsort(config, enabled_count, sizeof(joyConfigSlot_s), _compare_joyConfigSlot_by_angle);
    }
    
    return enabled_count;
}

/**
 * @brief Performs linear scaling on input angle and distance, correctly accounting for deadzones.
 * @param in_angle The raw input angle (0-360).
 * @param in_distance The raw input distance (0-2048 range).
 * @param config The array of calibration slots.
 * @param out_angle Pointer to store the resulting output angle.
 * @param out_distance Pointer to store the resulting output distance.
 * @return true if scaling was successful, false if no enabled slots are found.
 */
bool _joy_scale_input(int16_t *input, analog_meta_s *output, const joyConfigSlot_s config[ADJUSTABLE_ANGLES], float *out_angle, float *out_distance)
{
    int index_a, index_b;

    float in_angle = stick_scaling_coordinates_to_angle(input[0], input[1]);
    float in_distance = stick_scaling_coordinate_distance(input[0], input[1]);

    // 1. Find the two calibration points (a and b) that bracket the input angle.
    // NOTE: Requires 'joy_find_bracketing_indices' to be defined and functional.
    if (!_joy_find_bracketing_indices(in_angle, config, &index_a, &index_b))
    {
        return false; 
    }

    const joyConfigSlot_s *slot_a = &config[index_a];
    const joyConfigSlot_s *slot_b = &config[index_b];

    // --- ANGLE SCALING SETUP ---
    float a_in_angle = slot_a->in_angle;
    float b_in_angle = slot_b->in_angle;

    // 2. Normalize Angle Range (0 to < 360)
    // The total angular range is the shortest arc between A and B.
    float total_arc = b_in_angle - a_in_angle;
    if (total_arc < 0) total_arc += 360.0f; 

    // 3. Define Deadzone Halves
    float dz_a_half = slot_a->deadzone / 2.0f;
    float dz_b_half = slot_b->deadzone / 2.0f;

    // 4. Calculate Effective Interpolation Range
    
    // Check for the extreme case where the two deadzones overlap or touch.
    if (dz_a_half + dz_b_half >= total_arc)
    {
        // Deadzone overlap or complete coverage. Input should snap to A or B.
        // We'll choose the closest one to snap to.
        float diff_a = fmodf(in_angle - a_in_angle + 180.0f + 360.0f, 360.0f) - 180.0f;
        float diff_b = fmodf(in_angle - b_in_angle + 180.0f + 360.0f, 360.0f) - 180.0f;
        
        if (fabsf(diff_a) < fabsf(diff_b))
        {
             *out_angle = slot_a->out_angle;
        }
        else
        {
            *out_angle = slot_b->out_angle;
        }
        
        // Skip interpolation and go straight to distance scaling
        goto distance_scaling;
    }
    
    // The start of the linear interpolation range is A_in + dz_a_half
    // The end of the linear interpolation range is B_in - dz_b_half

    // Effective start angle for interpolation (relative to A_in):
    float effective_in_start = a_in_angle + dz_a_half; 
    
    // Effective input range for linear scaling:
    float effective_in_range = total_arc - dz_a_half - dz_b_half;

    // 5. Deadzone Check and Snap
    
    // Calculate the shortest signed angular difference from in_angle to effective_in_start.
    // The start angle is A_in + dz_a_half.
    float diff_from_start = fmodf(in_angle - effective_in_start + 180.0f + 360.0f, 360.0f) - 180.0f;

    if (diff_from_start <= 0.0f) 
    {
        // Input is within the deadzone of slot_a (or exactly at the start of the effective range)
        *out_angle = slot_a->out_angle;
        goto distance_scaling;
    }
    
    if (diff_from_start >= effective_in_range)
    {
        // Input is within the deadzone of slot_b (or exactly at the end of the effective range)
        *out_angle = slot_b->out_angle;
        goto distance_scaling;
    }
    
    // 6. Linear Interpolation (Lerp) over the Effective Range
    
    // Fraction 't' = (in_angle - effective_in_start) / effective_in_range
    float t = diff_from_start / effective_in_range;
    
    // Angle Lerp: out = a + t * (b - a)
    // The linear scaling is done between slot_a's out_angle and slot_b's out_angle.
    *out_angle = slot_a->out_angle + t * (slot_b->out_angle - slot_a->out_angle);
    
    // Ensure output angle is in the [0, 360) range
    *out_angle = fmodf(*out_angle + 360.0f, 360.0f);


distance_scaling:
    // --- DISTANCE SCALING ---
    // The distance scaling still uses the same interpolation factor 't' derived from the angle.
    // If the angle snapped, 't' is effectively 0.0 or 1.0 (or we skip distance scaling here 
    // and just use the snapped slot's distance ratio).
    
    float a_out_dist = slot_a->out_distance;
    float b_out_dist = slot_b->out_distance;
    
    // Determine the distance scaling factor (r)
    float r_a = slot_a->out_distance / (slot_a->in_distance > 0.0f ? slot_a->in_distance : 1.0f);
    float r_b = slot_b->out_distance / (slot_b->in_distance > 0.0f ? slot_b->in_distance : 1.0f);

    // If we snapped to a deadzone, 't' should be considered 0.0 or 1.0 for distance.
    if (*out_angle == slot_a->out_angle) t = 0.0f;
    else if (*out_angle == slot_b->out_angle) t = 1.0f;
    // Otherwise, 't' is the calculated interpolation factor from the angle scaling.

    // Linearly interpolate the *output distance ratio*
    float out_ratio = r_a + t * (r_b - r_a);

    // Apply the interpolated ratio to the input distance
    *out_distance = in_distance * out_ratio;
    
    // Clamp the output distance to the maximum range (2048)
    if (*out_distance > 2048.0f) *out_distance = 2048.0f;
    if (*out_distance < 0.0f) *out_distance = 0.0f;

    return true;
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
    out[0] = fmaxf(-2048, fminf(2047, out[0]));
    out[1] = fmaxf(-2048, fminf(2047, out[1]));
}

float stick_scaling_coordinates_to_angle(int x, int y) 
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

// Get distance to coordinates based on a 0, 0 center
float stick_scaling_coordinate_distance(int x, int y) {
    // Use Pythagorean theorem to calculate distance
    // Convert to float to ensure floating-point precision
    return sqrtf((float)(x * x + y * y));
}

bool _calibrate_axis(int16_t *in, joyConfigSlot_s slots[ADJUSTABLE_ANGLES])
{
  // Get input distance
  float distance  = stick_scaling_coordinate_distance(in[0], in[1]);
  // Get input angle
  float angle     = stick_scaling_coordinates_to_angle(in[0], in[1]);

  // Get slot number
  int idx = _joy_get_closest_enabled_index(angle, slots);
  if(idx<0) return false;

  // Assign distance if needed
  if(distance > slots[idx].in_distance)
  {
    slots[idx].in_distance = distance;
  }

  // Return false if an enabled angle is less than 400 distance for the input
  for(int i = 0; i < ADJUSTABLE_ANGLES; i++)
  {
    if((slots[i].in_distance < 400) && slots[i].enabled) return false;
  }

  return true;
}

// Write distance data to config block
void _write_to_config_block()
{

}
   
// Read parameters from config block
void _read_from_config_block() 
{

}

void _zero_distances(joyConfigSlot_s *slot)
{
  for(int i = 0; i < ADJUSTABLE_ANGLES; i++)
  {
    slot[i].in_distance = 256;
    slot[i].out_distance = 2048;
  }
}

void _set_default_configslot(joyConfigSlot_s *slots)
{
    for(int i = 0; i < ADJUSTABLE_ANGLES; i++)
    {
        if(i<8)
        {
            slots[i].deadzone = 2.0f; // 2 degree deadzone default
            slots[i].enabled = true;
            slots[i].in_angle = i*45.0f;
            slots[i].out_angle = i*45.0f;
            slots[i].in_distance = 2048.0f;
            slots[i].out_distance = 2048.0f;
        }
        else 
        {
            slots[i].enabled = false;
        }
    }
}

void stick_scaling_calibrate_start(bool start)
{
  MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);
  if(!_sticks_calibrating && start) 
  {
    // Reset all to default
    _zero_distances(analog_config->joy_config_l);
    _zero_distances(analog_config->joy_config_r);

    hoja_set_notification_status(COLOR_RED);

    _sticks_calibrating = true;
  }
  else if (_sticks_calibrating && !start) 
  {
    hoja_set_notification_status(COLOR_BLACK);
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

    // Set default deadzones
    analog_config->l_deadzone = 160;
    analog_config->r_deadzone = 160;

    analog_config->l_deadzone_outer = 0;
    analog_config->r_deadzone_outer = 0;

    analog_config->lx_center = 2048;
    analog_config->ly_center = 2048;
    analog_config->rx_center = 2048;
    analog_config->ry_center = 2048;

    analog_config->l_snapback_intensity = 600; // 60.0hz
    analog_config->r_snapback_intensity = 600;

    analog_config->lx_invert = 0;
    analog_config->ly_invert = 0;
    analog_config->rx_invert = 0;
    analog_config->ry_invert = 0;

    analog_config->l_snapback_type = 0;
    analog_config->r_snapback_type = 0;

    // Set to default left setup
    _set_default_configslot(analog_config->joy_config_l);
    _set_default_configslot(analog_config->joy_config_r);
  }
}

// Input and output are based around -2048 to 2048 values with 0 as centers
void stick_scaling_process(analog_data_s *in, analog_data_s *out)
{
  static int16_t in_left[2]    = {0};
  static int16_t in_right[2]   = {0};

  in_left[0] = in->lx;
  in_left[1] = in->ly;

  in_right[0] = in->rx;
  in_right[1] = in->ry;

  // Float angle and distance data
  analog_meta_s out_left_meta  = {0};
  analog_meta_s out_right_meta = {0};

  int16_t out_left[2];
  int16_t out_right[2];

  MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);
  if(_sticks_calibrating)
  {
    bool left_done  = false; //_calibrate_axis(in_left, &_left_setup);
    bool right_done = false; //_calibrate_axis(in_right, &_right_setup);
    out_left[0] = 0;
    out_left[1] = 0;
    out_right[0] = 0;
    out_right[1] = 0;

    if(left_done && right_done)
    {
      hoja_set_notification_status(COLOR_CYAN);
    }
  }
  else 
  {
    _process_axis(in_left,  &out_left_meta,   &_left_setup);
    _process_axis(in_right, &out_right_meta,  &_right_setup);
    analog_angle_distance_to_coordinate(out_left_meta.angle,  out_left_meta.distance,   out_left);
    analog_angle_distance_to_coordinate(out_right_meta.angle, out_right_meta.distance,  out_right);
  }

  out->lx = out_left[0];
  out->ly = out_left[1];
  out->rx = out_right[0];
  out->ry = out_right[1];

  out->langle     = out_left_meta.angle;
  out->ldistance  = out_left_meta.distance;
  out->ltarget    = out_left_meta.target;

  out->rangle     = out_right_meta.angle;
  out->rdistance  = out_right_meta.distance;
  out->rtarget    = out_right_meta.target;

  MUTEX_HAL_EXIT(&_stick_scaling_mutex);
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
