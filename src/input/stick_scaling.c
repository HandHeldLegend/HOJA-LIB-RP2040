#include "stick_scaling.h"

#define STICK_INTERNAL_CENTER 2048
#define STICK_MAX 4095
#define STICK_DEAD_ZONE 24
#define STICK_CALIBRATION_DEADZONE 125
#define STICK_SCALE_DISTANCE STICK_INTERNAL_CENTER

#define CLAMP_0_MAX(value) ((value) < 0 ? 0 : ((value) > STICK_MAX ? STICK_MAX : (value)))

const float _angle_lut[8] = {0, 45, 90, 135, 180, 225, 270, 315};

// E, NE, N, NW, W, SW, S, SE
float _stick_l_calibrated_angles[8] = {0, 0, 0, 0, 0, 0, 0, 0};
float _stick_l_calibrated_distances[8] = {600, 600, 600, 600, 600, 600, 600, 600};
float _stick_l_distance_scalers[8] = {1, 1, 1, 1, 1, 1, 1, 1};

float _stick_r_calibrated_angles[8] = {0, 0, 0, 0, 0, 0, 0, 0};
float _stick_r_calibrated_distances[8] = {600, 600, 600, 600, 600, 600, 600, 600};
float _stick_r_distance_scalers[8] = {1, 1, 1, 1, 1, 1, 1, 1};

int _stick_l_center_x;
int _stick_l_center_y;
int _stick_r_center_x;
int _stick_r_center_y;

// Adjusts angle by the adjustment parameter
// and returns the appropriate angle (loops around if needed)
float _stick_angle_adjust(float angle, float adjustment)
{
  float out = angle + adjustment;
  if (out > 360.0f)
    out -= 360;
  else if (out < 0)
    out += 360;
  return out;
}

bool _stick_check_between_angles(float input, float angle1, float angle2)
{
  if (angle1 > angle2) // Edge case where angle 0 is a negative adjustment. (greater than)
    return (input >= angle1) && (input < angle2);
  else
    return (input > angle2) && (input <= angle1);
}

// Returns the octant where the angle is
int _stick_get_octant_adjusted(float angle)
{
  return (int)_stick_angle_adjust(angle, 22.5f) / 45;
}

int _stick_get_octant(float angle, float *angles)
{
  float a1 = 0;
  float a2 = 0;
  for (uint8_t i = 0; i < 8; i++)
  {
    uint8_t o = i + 1;

    if (i == 7)
    {
      return 7;
    }

    if (i == 0)
    {
      a1 = _stick_angle_adjust(_angle_lut[0], angles[0]);
      a2 = _stick_angle_adjust(_angle_lut[1], angles[1]);
      if (a1 > a2) // Edge case where angle 0 is a negative adjustment
      {
        if ((angle >= a1) || (angle < a2))
        {
          return 0;
        }
      }
      else // Edge case where angle 0 is a positive adjustment or no adjustment
      {
        if ((angle >= a1) && (angle < a2))
        {
          return 0;
        }
      }
    }
    else
    {
      a1 = _stick_angle_adjust(_angle_lut[i], angles[i]);
      a2 = _stick_angle_adjust(_angle_lut[o], angles[o]);
      if ((angle >= a1) && (angle < a2))
      {
        return i;
      }
    }
  }

  return 0;
}

// Returns the float angle given the XY coordinate pair and the
// calibrated center point
float _stick_get_angle(int x, int y, int center_x, int center_y)
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

// Returns float distance between float coordinate pair
// and 512,512 coordinate center point
float _stick_get_distance(int x, int y, int center_x, int center_y)
{
  float dx = (float)x - (float)center_x;
  float dy = (float)y - (float)center_y;
  return sqrtf(dx * dx + dy * dy);
}

// Produces a normalized vector at a given angle
void _stick_normalized_vector(float angle, float *x, float *y)
{
  float rad = angle * (M_PI / 180.0);
  *x = cos(rad);
  *y = sin(rad);
}

// Calculates the new angle output based on angle and distance
void _stick_angle_distance_scaled(float angle, float distance, float *angles, float *scalers, float *out_x, float *out_y)
{
  int o = _stick_get_octant(angle, &angles[0]); // Get octant the stick is in

  int o2 = o + 1;
  if (o == 7)
    o2 = 0;

  float a2 = _stick_angle_adjust(_angle_lut[o2], angles[o2]);
  float a1 = _stick_angle_adjust(_angle_lut[o], angles[o]);

  float d = _stick_angle_adjust(a2, -a1);    // Get distance of calibrated angle points
  float a = _stick_angle_adjust(angle, -a1); // Get normalized angle distance

  float p = a / d;     // Get the percent distance we are currently
  float p2 = 1.0f - p; // Get other percent remainder

  float s = (scalers[o2] * p) + (scalers[o] * p2);

  float na = (p * 45) + (o * 45);

  float nx = 0;
  float ny = 0;
  _stick_normalized_vector(na, &nx, &ny);

  float nd = distance * s;
  nx *= nd;
  ny *= nd;

  *out_x = CLAMP_0_MAX((int)roundf(nx + STICK_INTERNAL_CENTER));
  *out_y = CLAMP_0_MAX((int)roundf(ny + STICK_INTERNAL_CENTER));
  ;
}

// Calculate the angular scalers used to determine the distance scaler
void _stick_calculate_distance_scalers(float *distances_in, float *scalers_out)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    scalers_out[i] = (STICK_SCALE_DISTANCE) / (distances_in[i]);
  }
}

// PUBLIC FUNCTIONS
// Load stick scaling settings from global_loaded_settings
void stick_scaling_get_settings()
{
  // Copy values from settings to working mem here
  memcpy(_stick_l_calibrated_distances, global_loaded_settings.l_angle_distances, sizeof(float) * 8);
  memcpy(_stick_r_calibrated_distances, global_loaded_settings.r_angle_distances, sizeof(float) * 8);
  _stick_l_center_x = global_loaded_settings.lx_center;
  _stick_r_center_x = global_loaded_settings.rx_center;
  _stick_l_center_y = global_loaded_settings.ly_center;
  _stick_r_center_y = global_loaded_settings.ry_center;

  memcpy(_stick_l_calibrated_angles, global_loaded_settings.l_angles, sizeof(float) * 8);
  memcpy(_stick_r_calibrated_angles, global_loaded_settings.r_angles, sizeof(float) * 8);
}

// Copies working calibration data
// to global_loaded_settings
void stick_scaling_set_settings()
{
  settings_set_centers(_stick_l_center_x, _stick_l_center_y, _stick_r_center_x, _stick_r_center_y);
  settings_set_distances(_stick_l_calibrated_distances, _stick_r_calibrated_distances);
  settings_set_angles(_stick_l_calibrated_angles, _stick_r_calibrated_angles);
}

// Performs the math to create the
// scaling values from the loaded data
void stick_scaling_init()
{
  _stick_calculate_distance_scalers(_stick_l_calibrated_distances, _stick_l_distance_scalers);
  _stick_calculate_distance_scalers(_stick_r_calibrated_distances, _stick_r_distance_scalers);
}

uint16_t _stick_distances_tracker = 0x00;

void stick_scaling_reset_distances()
{
  _stick_distances_tracker = 0x00;
  memset(_stick_l_calibrated_distances, 0, sizeof(float) * 8);
  memset(_stick_r_calibrated_distances, 0, sizeof(float) * 8);
  memset(_stick_l_calibrated_angles, 0, sizeof(float) * 8);
  memset(_stick_r_calibrated_angles, 0, sizeof(float) * 8);
}

bool stick_scaling_capture_distances(a_data_s *in)
{
  // Get angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  int lo = _stick_get_octant_adjusted(la);
  int ro = _stick_get_octant_adjusted(ra);

  // Get distance of input
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  float l_a_d = fmod(la, 45);
  if ((l_a_d < 1) || (l_a_d > 44))
  {
    if ((ld > _stick_l_calibrated_distances[lo]) && (ld > STICK_CALIBRATION_DEADZONE))
    {
      _stick_l_calibrated_distances[lo] = ld;
      _stick_distances_tracker |= (1 << lo);
    }
  }

  float r_a_d = fmod(ra, 45);
  if ((r_a_d < 1) || (r_a_d > 44))
    if ((rd > _stick_r_calibrated_distances[ro]) && (rd > STICK_CALIBRATION_DEADZONE))
    {
      _stick_r_calibrated_distances[ro] = rd;
      _stick_distances_tracker |= (1 << (ro + 8));
    }

  return (_stick_distances_tracker == 0xFFFF);
}

void stick_scaling_capture_center(a_data_s *in)
{
  _stick_l_center_x = in->lx;
  _stick_l_center_y = in->ly;

  _stick_r_center_x = in->rx;
  _stick_r_center_y = in->ry;
}

// Captures a stick angle for octagon calibration
bool stick_scaling_capture_angle(a_data_s *in)
{
  // Get angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get distance of inputs
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  if (ld > STICK_CALIBRATION_DEADZONE)
  {
    int lo = _stick_get_octant_adjusted(la);

    if (!lo)
    {
      if (la >= 315)
      {
        // Set the new angle offset
        _stick_l_calibrated_angles[0] = (la-360);
      }
      else
      {
        // Set the new angle offset
        _stick_l_calibrated_angles[0] = la;
      }
    }
    else
    {
      // Set the new angle offset
      _stick_l_calibrated_angles[lo] = la - _angle_lut[lo];
    }

    // Set the new angle distance
    _stick_l_calibrated_distances[lo] = ld;

    return true;
  }
  else if (rd > STICK_CALIBRATION_DEADZONE)
  {
    int ro = _stick_get_octant_adjusted(ra);

    if (!ro)
    {
      if (ra >= 315)
      {
        // Set the new angle offset
        _stick_r_calibrated_angles[0] = ra - 360;
      }
      else
      {
        // Set the new angle offset
        _stick_r_calibrated_angles[0] = ra;
      }
    }
    else
    {
      // Set the new angle offset
      _stick_r_calibrated_angles[ro] = ra - _angle_lut[ro];
    }

    // Set the new angle distance
    _stick_r_calibrated_distances[ro] = rd;

    return true;
  }

  return false;
}

void stick_scaling_process_data(a_data_s *in, a_data_s *out)
{
  // Get angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get distance of input
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  float lx = 0;
  float ly = 0;
  float rx = 0;
  float ry = 0;

  _stick_angle_distance_scaled(la, ld, _stick_l_calibrated_angles, _stick_l_distance_scalers, &lx, &ly);
  _stick_angle_distance_scaled(ra, rd, _stick_r_calibrated_angles, _stick_r_distance_scalers, &rx, &ry);

  out->lx = lx;
  out->rx = rx;
  out->ly = ly;
  out->ry = ry;
}
