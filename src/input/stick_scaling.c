/**
 * @file stick_scaling.c
 * @brief This file contains the implementation of stick scaling functions.
 */

#include "stick_scaling.h"

#define STICK_INTERNAL_CENTER 2048
#define STICK_MAX 4095

#define STICK_CALIBRATION_DEADZONE 125
#define STICK_SCALE_DISTANCE STICK_INTERNAL_CENTER

#define CLAMP_0_MAX(value) ((value) < 0 ? 0 : ((value) > STICK_MAX ? STICK_MAX : (value)))
#define CLAMP_FLOAT_DISTANCE(value) ((value) < 0 ? 0 : ((value) > STICK_INTERNAL_CENTER ? STICK_INTERNAL_CENTER : (value)))

// Direct cardinal angles
const float _angle_lut[8] = {0, 45, 90, 135, 180, 225, 270, 315};

// Our base angles for scaling
const float _base_angles_lut[8] = {337.5, 22.5, 67.5, 112.5, 157.5, 202.5, 247.5, 292.5};

// Precalculated distance scalers
float _stick_l_distance_scalers[8] = {1, 1, 1, 1, 1, 1, 1, 1};
float _stick_r_distance_scalers[8] = {1, 1, 1, 1, 1, 1, 1, 1};

// Precalculated angle scalers
float _stick_l_angle_scalers[8] = {1,1,1,1,1,1,1,1};
float _stick_r_angle_scalers[8] = {1,1,1,1,1,1,1,1};

int _stick_l_center_x;
int _stick_l_center_y;

int _stick_r_center_x;
int _stick_r_center_y;

uint8_t _stick_deadzone_outer_l;
uint8_t _stick_deadzone_outer_r;

typedef struct
{
  bool set;
  float scale_lower;
  float scale_upper;
  float parting_angle;
} angle_sub_scale_s;

angle_sub_scale_s _l_sub_angle_states[8] = {0};
angle_sub_scale_s _r_sub_angle_states[8] = {0};



float _stick_sub_angle_scale(float angle, angle_sub_scale_s *state)
{
  if(!state->set) return angle;

  if(angle <= state->parting_angle)
  {
    return angle * state->scale_lower;
  }
  else return ( (angle - state->parting_angle) * state->scale_upper )+22.5f;
}

/**
 * Calculates the distance between two angles in degrees.
 *
 * @param angle1 The first angle in degrees.
 * @param angle2 The second angle in degrees.
 * @return The distance between the two angles.
 */
float _get_angle_distance(float angle1, float angle2) {
  // Calculate the absolute difference
  float diff = fabs(angle1 - angle2);

  // Take the shorter distance considering the circular range
  float distance = fmin(diff, 360.0 - diff);

  return distance;
}

/**
 * Checks if an angle is between two given angles.
 *
 * @param angle The angle to check.
 * @param a1 The first angle of the range.
 * @param a2 The second angle of the range.
 * @return true if the angle is between a1 and a2 (inclusive), false otherwise.
 */
bool _is_angle_between(float angle, float a1, float a2)
{
  // Ensure startAngle is less than endAngle
  if (a1 <= a2) {
      return angle >= a1 && angle <= a2;
  } else {
      // Handle the case where the range wraps around (e.g., startAngle = 300, endAngle = 30)
      return angle >= a1 || angle <= a2;
  }
}

/**
 * Returns the octant based on the processed data.
 *
 * This function takes an angle and an array of angles representing the boundaries of each octant.
 * It iterates through the array and checks if the given angle falls between two consecutive angles.
 * If the angle is found to be between two consecutive angles, the corresponding octant index is returned.
 *
 * @param angle The angle to be processed.
 * @param c_angles An array of angles representing the boundaries of each octant.
 * @return The octant index based on the processed data.
 */
int _stick_get_processed_octant(float angle, float *c_angles)
{
  int out = 0;

  for(uint8_t i = 0; i<8; i++)
  {
    int t2 = (i<7) ? i+1 : 0;

    if(_is_angle_between(angle, c_angles[i], c_angles[t2])) out = i;
  }

  return out;
}

/**
 * Returns an octant of 0 through 7 to indicate the cardinal direction.
 *
 * @param angle The angle in degrees.
 * @return The octant value representing the cardinal direction.
 */
int _stick_get_octant(float angle)
{
  // Ensure the angle is in the range [0, 360)
    angle = fmod(angle, 360.0);
    if (angle < 0) {
        angle += 360.0;
    }

    // Compute the octant
    int octant = (int)((angle) / 45.0) % 8;

    return octant;
}


/**
 * @brief Adjusts the given angle by the adjustment parameter and returns the appropriate angle.
 * 
 * This function takes an angle value and adjusts it by the given adjustment parameter.
 * If the adjusted angle exceeds 360 degrees, it wraps around to the range of 0 to 360 degrees.
 * If the adjusted angle is less than 0 degrees, it wraps around to the range of 0 to 360 degrees.
 * 
 * Angle order E, NE, N, NW, W, SW, S, SE
 * 
 * @param angle The original angle value.
 * @param adjustment The adjustment value to be added to the original angle.
 * @return The adjusted angle value.
 */
float _stick_angle_adjust(float angle, float adjustment)
{
  // Adjust the angle by adding the adjustment parameter
  float out = angle + adjustment;
  
  // Wrap the adjusted angle around to the range of 0 to 360 degrees
  if (out > 360.0f)
    out -= 360;
  else if (out < 0)
    out += 360;
  return out;
}

/**
 * Calculates the angle in degrees given the XY coordinate pair and the calibrated center point.
 *
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param center_x The X coordinate of the center point.
 * @param center_y The Y coordinate of the center point.
 * @return The angle in degrees.
 */
float stick_get_angle(int x, int y, int center_x, int center_y)
{
  float angle = atan2f((y - center_y), (x - center_x)) * 180.0f / M_PI;

  if (angle < 0)
  {
    angle += 360.0f;
  }
  else if (angle >= 360.0f)
  {
    angle -= 360.0f;
  }
  return angle;
}

/**
 * Calculates the distance between a coordinate pair (x, y) and a center point (center_x, center_y).
 *
 * @param x The x-coordinate of the point.
 * @param y The y-coordinate of the point.
 * @param center_x The x-coordinate of the center point.
 * @param center_y The y-coordinate of the center point.
 * @return The distance between the point and the center point.
 */
float stick_get_distance(int x, int y, int center_x, int center_y)
{
  float dx = (float)x - (float)center_x;
  float dy = (float)y - (float)center_y;
  return sqrtf(dx * dx + dy * dy);
}

/**
 * Calculates the normalized vector components (x, y) at a given angle.
 *
 * @param angle The angle in degrees.
 * @param x     Pointer to store the x-component of the normalized vector.
 * @param y     Pointer to store the y-component of the normalized vector.
 */
void stick_normalized_vector(float angle, float *x, float *y)
{
  float rad = angle * (M_PI / 180.0);
  *x = cos(rad);
  *y = sin(rad);
}

/**
 * Calculates the new angle and distance output based on the given input angle and distance.
 *
 * @param angle The input angle.
 * @param distance The input distance.
 * @param c_angles The array of calibrated angles.
 * @param d_scalers The array of distance scalers.
 * @param a_scalers The array of angle scalers.
 * @param sub_state The array of sub-state for angle adjustment.
 * @param out_x Pointer to store the output x-coordinate.
 * @param out_y Pointer to store the output y-coordinate.
 */
void _stick_process_input(float angle, float distance, float *c_angles, float *d_scalers, float *a_scalers, angle_sub_scale_s *sub_state, float *out_x, float *out_y)
{
  // First get the octant
  // This will determine which scalers we are using
  // By not shifting the angle 22.5 degrees, this octant is aligned with the direct cardinals.
  int _o = _stick_get_processed_octant(angle, c_angles);
  // Get the second octant which is loops around if we are in octant 7
  int _o2 = (_o < 7) ? _o+1 : 0;

  // Store total distance between 
  // calibrated angles
  float _td = _get_angle_distance(c_angles[_o], c_angles[_o2]);

  // Get how far along the angle is along the path
  float _ad = _get_angle_distance(angle, c_angles[_o]);

  // Get ratio of how far along the angle is compared to the total distance
  float _str = _ad / _td;

  // Generate appropriate scaler value
  float _new_d_scaler = (d_scalers[_o] * (1.0f-_str)) + (d_scalers[_o2] * _str);
  
  // Get scaled angle
  float _sa = _ad * a_scalers[_o];

  // Perform sub-adjust if we need to.
  if(sub_state[_o].set)
  {
    _sa = _stick_sub_angle_scale(_sa, &(sub_state[_o]));
  }

  // Add scaled angle to create final angle
  float _fa = _stick_angle_adjust(_angle_lut[_o], _sa);

  float nx = 0;
  float ny = 0;

  stick_normalized_vector(_fa, &nx, &ny);

  float nd = distance * _new_d_scaler;
  nd = (float) CLAMP_FLOAT_DISTANCE(nd);
  nx *= nd;
  ny *= nd;

  *out_x = CLAMP_0_MAX((int)roundf(nx + STICK_INTERNAL_CENTER));
  *out_y = CLAMP_0_MAX((int)roundf(ny + STICK_INTERNAL_CENTER));
}

// PRECALCULATE FUNCTIONS

  /**
   * Precalculates the distance scalers based on the input distances.
   *
   * @param distances_in The input distances.
   * @param scalers_out The output scalers.
   */
  void _precalculate_distance_scalers(float *distances_in, float *scalers_out, uint16_t outer_deadzone)
  {
    for (uint8_t i = 0; i < 8; i++)
    {
      scalers_out[i] = (STICK_SCALE_DISTANCE+(float) outer_deadzone) / (distances_in[i]);
    }
  }

  /**
   * Precalculates the angle scalers based on the input angles.
   *
   * @param angles_in The input angles array.
   * @param scalers_out The output scalers array.
   */
  void _precalculate_angle_scalers(float *angles_in, float *scalers_out)
  {
    for(uint8_t i = 0; i < 8; i++)
    {
      int _a2 = (i<7) ? i + 1 : 0;
      float _d = _get_angle_distance(angles_in[i], angles_in[_a2]);
      
      // Create a scaler to 45 degrees based on the distance
      float _as = 45.0f/_d;

      scalers_out[i] = _as;
    }
  }

  /**
   * Precalculates the sub-scaler states based on the input sub-angles.
   * 
   * @param sub_angles_in The array of sub-angles.
   * @param state The array of angle_sub_scale_s structures to store the calculated states.
   */
  void _precalculate_angle_substate(float *sub_angles_in, angle_sub_scale_s *state)
  {
    for(uint8_t i = 0; i < 8; i++)
    {
      if(sub_angles_in[i] == 0)
      {
        // If sub-angle is 0, set scaler values to default values
        state[i].set = false;
        state[i].parting_angle = 22.5f;
        state[i].scale_lower = 1;
        state[i].scale_upper = 1;
      }
      else
      {
        // If sub-angle is not 0, calculate scaler values based on the sub-angle
        state[i].set = true;
        state[i].parting_angle = 22.5 + sub_angles_in[i];
        state[i].scale_lower = (22.5f / state[i].parting_angle);
        state[i].scale_upper = (22.5f / (45.0f - state[i].parting_angle) );
      }
    }
  }

// PUBLIC FUNCTIONS
// Load stick scaling settings from global_loaded_settings
void stick_scaling_get_settings()
{
  // Copy values from settings to working mem here
  _stick_l_center_x = global_loaded_settings.lx_center.center;
  _stick_r_center_x = global_loaded_settings.rx_center.center;
  _stick_l_center_y = global_loaded_settings.ly_center.center;
  _stick_r_center_y = global_loaded_settings.ry_center.center;
}

// Copies working calibration data
// to global_loaded_settings
void stick_scaling_set_settings()
{
  settings_set_centers(_stick_l_center_x, _stick_l_center_y, _stick_r_center_x, _stick_r_center_y);
}

// Performs the math to create the
// scaling values from the loaded data
void stick_scaling_init()
{
  _precalculate_angle_scalers(global_loaded_settings.l_angles, _stick_l_angle_scalers);
  _precalculate_angle_scalers(global_loaded_settings.r_angles, _stick_r_angle_scalers);

  _precalculate_distance_scalers(global_loaded_settings.l_angle_distances, _stick_l_distance_scalers, global_loaded_settings.deadzone_left_outer);
  _precalculate_distance_scalers(global_loaded_settings.r_angle_distances, _stick_r_distance_scalers, global_loaded_settings.deadzone_right_outer);

  _precalculate_angle_substate(global_loaded_settings.l_sub_angles, _l_sub_angle_states);
  _precalculate_angle_substate(global_loaded_settings.r_sub_angles, _r_sub_angle_states);
}

uint16_t _stick_distances_tracker = 0x00;

/**
 * @brief Resets the distances and angles for stick scaling.
 * 
 * This function resets the distances and angles used for stick scaling.
 * It sets the stick distances tracker to 0x00 and clears the arrays
 * for left and right angle distances, left and right angles.
 */
void stick_scaling_reset_distances()
{
  _stick_distances_tracker = 0x00;
  memset(global_loaded_settings.l_angle_distances, 0, sizeof(float) * 8);
  memset(global_loaded_settings.r_angle_distances, 0, sizeof(float) * 8);
  memset(global_loaded_settings.l_angles, 0, sizeof(float) * 8);
  memset(global_loaded_settings.r_angles, 0, sizeof(float) * 8);
}

/**
 * @brief Captures the distances and angles of the input stick.
 *
 * This function captures the angles and distances of the input stick and updates the calibrated
 * distances and angles in the global settings. It also updates the stick distances tracker.
 *
 * @param in Pointer to the input data structure.
 * @return Returns true if all distances have been captured, false otherwise.
 */
bool stick_scaling_capture_distances(a_data_s *in)
{
  // Get angles of input
  float la = stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Adjust angles
  float sla = _stick_angle_adjust(la, 22.5f);
  float sra = _stick_angle_adjust(ra, 22.5f);

  int lo = _stick_get_octant(sla);
  int ro = _stick_get_octant(sra);

  // Get distance of input
  float ld = stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);


  float l_a_d = fmod(la, 45);
  if ((l_a_d < 1) || (l_a_d > 44))
  {
    if ((ld > global_loaded_settings.l_angle_distances[lo]) && (ld > STICK_CALIBRATION_DEADZONE))
    {

      // Update calibrated distance
      global_loaded_settings.l_angle_distances[lo] = ld;

      // Update calibrated angle
      global_loaded_settings.l_angles[lo] = la;

      _stick_distances_tracker |= (1 << lo);
    }
  }

  float r_a_d = fmod(ra, 45);
  if ((r_a_d < 1) || (r_a_d > 44))
  {
    if ((rd > global_loaded_settings.r_angle_distances[ro]) && (rd > STICK_CALIBRATION_DEADZONE))
    {
      // Update calibrated distance
      global_loaded_settings.r_angle_distances[ro] = rd;

      // Update calibrated angle
      global_loaded_settings.r_angles[ro] = ra;

      _stick_distances_tracker |= (1 << (ro + 8));
    }
  }

  return (_stick_distances_tracker == 0xFFFF);
}

/**
 * @brief Captures the center values of the stick inputs.
 *
 * This function captures the center values of the stick inputs and stores them in the corresponding variables.
 *
 * @param in Pointer to the input data structure.
 */
void stick_scaling_capture_center(a_data_s *in)
{
  _stick_l_center_x = in->lx;
  _stick_l_center_y = in->ly;

  _stick_r_center_x = in->rx;
  _stick_r_center_y = in->ry;
}

/**
 * @brief Determines the octant and axis of the stick input.
 *
 * This function calculates the angles and distances of the stick inputs and determines
 * the octant and axis based on the calculated values. The octant represents the direction
 * in which the stick is tilted, while the axis represents whether it is the left stick or
 * the right stick.
 *
 * @param in Pointer to the input data structure.
 * @param axis Pointer to store the axis value (0 for left stick, 1 for right stick).
 * @param octant Pointer to store the octant value (ranging from 0 to 7).
 */
void stick_scaling_get_octant_axis(a_data_s *in, uint8_t *axis, uint8_t *octant)
{
  // Get angles of input
  float la = stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get distance of inputs
  float ld = stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  if (ld > STICK_CALIBRATION_DEADZONE)
  {
    int lo = _stick_get_octant(la);
    *axis = 0;
    *octant = (uint8_t) lo;
  }
  else if (rd > STICK_CALIBRATION_DEADZONE)
  {
    int ro = _stick_get_octant(ra);
    *axis = 1;
    *octant = (uint8_t) ro;
  }
}

/**
 * @brief Calculates the axis and octant offset based on stick input values.
 *
 * This function calculates the axis and octant offset based on the stick input values.
 * It first calculates the angles of the input values using the stick center coordinates.
 * Then, it adjusts the angles by 22.5 degrees.
 * Next, it calculates the distance of the input values from the stick center coordinates.
 * If the left stick distance is greater than the deadzone threshold, it calculates the octant of the adjusted left angle.
 * If the right stick distance is greater than the deadzone threshold, it calculates the octant of the adjusted right angle.
 * Finally, it assigns the axis and octant values to the provided pointers.
 *
 * @param in Pointer to the input data structure.
 * @param axis Pointer to store the calculated axis value (0 for left stick, 1 for right stick).
 * @param octant Pointer to store the calculated octant value.
 */
void stick_scaling_get_octant_axis_offset(a_data_s *in, uint8_t *axis, uint8_t *octant)
{
  // Get angles of input
  float la = stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  float sla = _stick_angle_adjust(la, 22.5);
  float sra = _stick_angle_adjust(ra, 22.5);

  // Get distance of inputs
  float ld = stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  if (ld > STICK_CALIBRATION_DEADZONE)
  {
    int lo = _stick_get_octant(sla);
    *axis = 0;
    *octant = (uint8_t) lo;
  }
  else if (rd > STICK_CALIBRATION_DEADZONE)
  {
    int ro = _stick_get_octant(sra);
    *axis = 1;
    *octant = (uint8_t) ro;
  }
}

/**
 * @brief Captures a stick angle for octagon calibration.
 *
 * This function captures the stick angle for octagon calibration. It calculates the angles and distances of the input stick positions and adjusts them to ensure they are in the correct octant. 
 * If the distance of the left stick is greater than the deadzone threshold, it updates the angle and distance values for the corresponding octant in the global settings. 
 * If the distance of the right stick is greater than the deadzone threshold, it updates the angle and distance values for the corresponding octant in the global settings. 
 * After updating the values, it initializes the stick scaling and returns true. If neither stick distance is greater than the deadzone threshold, it returns false.
 * 
 * @param in Pointer to the input data structure.
 * @return Returns true if the stick angle was captured and updated, false otherwise.
 */
// Captures a stick angle for octagon calibration
bool stick_scaling_capture_angle(a_data_s *in)
{
  // Get angles of input
  float la = stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get angle adjusted to ensure we're adjusting the correct octant
  float sla = _stick_angle_adjust(la, 22.5f);
  float sra = _stick_angle_adjust(ra, 22.5f);

  // Get distance of inputs
  float ld = stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  if (ld > STICK_CALIBRATION_DEADZONE)
  {
    int lo = _stick_get_octant(sla);

    // Set new angle
    global_loaded_settings.l_angles[lo] = la;

    // Set the new angle distance
    global_loaded_settings.l_angle_distances[lo] = ld;

    stick_scaling_init();

    return true;
  }
  else if (rd > STICK_CALIBRATION_DEADZONE)
  {
    int ro = _stick_get_octant(sra);

    // Set new angle
    global_loaded_settings.r_angles[ro] = ra;

    // Set the new angle distance
    global_loaded_settings.r_angle_distances[ro] = rd;

    stick_scaling_init();

    return true;
  }

  return false;
}

/**
 * @brief Process the input data from the stick and scale the values accordingly.
 *
 * This function takes the input data from the stick, including the raw angles and distances,
 * and scales them based on the loaded settings and distance scalers. The processed values are
 * then stored in the output structure.
 *
 * @param in Pointer to the input data structure containing the raw stick angles and distances.
 * @param out Pointer to the output data structure where the processed stick values will be stored.
 */
void stick_scaling_process_data(a_data_s *in, a_data_s *out)
{
  // Get raw angles of input
  float la = stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get distance of input
  float ld = stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  float lx = 0;
  float ly = 0;
  float rx = 0;
  float ry = 0;

  _stick_process_input(la, ld, global_loaded_settings.l_angles, _stick_l_distance_scalers, _stick_l_angle_scalers, _l_sub_angle_states, &lx, &ly);
  _stick_process_input(ra, rd, global_loaded_settings.r_angles, _stick_r_distance_scalers, _stick_r_angle_scalers, _r_sub_angle_states, &rx, &ry);

  out->lx = global_loaded_settings.lx_center.invert ? (4095 - lx) : lx;
  out->ly = global_loaded_settings.ly_center.invert ? (4095 - ly) : ly;
  out->rx = global_loaded_settings.rx_center.invert ? (4095 - rx) : rx;
  out->ry = global_loaded_settings.ry_center.invert ? (4095 - ry) : ry;
}
