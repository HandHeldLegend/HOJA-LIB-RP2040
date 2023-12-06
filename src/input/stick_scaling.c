#include "stick_scaling.h"

#define STICK_INTERNAL_CENTER 2048
#define STICK_MAX 4095
#define STICK_DEAD_ZONE 24
#define STICK_CALIBRATION_DEADZONE 125
#define STICK_SCALE_DISTANCE STICK_INTERNAL_CENTER

#define CLAMP_0_MAX(value) ((value) < 0 ? 0 : ((value) > STICK_MAX ? STICK_MAX : (value)))

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

float _get_angle_distance(float angle1, float angle2) {
    // Calculate the absolute difference
    float diff = fabs(angle1 - angle2);

    // Take the shorter distance considering the circular range
    float distance = fmin(diff, 360.0 - diff);

    return distance;
}

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

// Returns octant based on processed data
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

// Returns an octant of 0 through 7 to indicate the cardinal direction.
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


// Uses an angle and returns the appropriate scaling value for the distance value
// Angle order 
// E, NE, N, NW, W, SW, S, SE

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
// and saved coordinate center point
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

// Calculates the new angle and distance output
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

  _stick_normalized_vector(_fa, &nx, &ny);

  float nd = distance * _new_d_scaler;
  nx *= nd;
  ny *= nd;

  *out_x = CLAMP_0_MAX((int)roundf(nx + STICK_INTERNAL_CENTER));
  *out_y = CLAMP_0_MAX((int)roundf(ny + STICK_INTERNAL_CENTER));
}

// PRECALCULATE FUNCTIONS

  // PRECalculate the distance scalers
  void _precalculate_distance_scalers(float *distances_in, float *scalers_out)
  {
    for (uint8_t i = 0; i < 8; i++)
    {
      scalers_out[i] = (STICK_SCALE_DISTANCE) / (distances_in[i]);
    }
  }

  // PRECalculate the angle scalers
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

  // PRECalculate sub-scaler states
  void _precalculate_angle_substate(float *sub_angles_in, angle_sub_scale_s *state)
  {
    for(uint8_t i = 0; i < 8; i++)
    {
      if(sub_angles_in[i] == 0)
      {
        state[i].set = false;
        state[i].parting_angle = 22.5f;
        state[i].scale_lower = 1;
        state[i].scale_upper = 1;
      }
      else
      {
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

  _precalculate_distance_scalers(global_loaded_settings.l_angle_distances, _stick_l_distance_scalers);
  _precalculate_distance_scalers(global_loaded_settings.r_angle_distances, _stick_r_distance_scalers);

  _precalculate_angle_substate(global_loaded_settings.l_sub_angles, _l_sub_angle_states);
  _precalculate_angle_substate(global_loaded_settings.r_sub_angles, _r_sub_angle_states);
}

uint16_t _stick_distances_tracker = 0x00;

void stick_scaling_reset_distances()
{
  _stick_distances_tracker = 0x00;
  memset(global_loaded_settings.l_angle_distances, 0, sizeof(float) * 8);
  memset(global_loaded_settings.r_angle_distances, 0, sizeof(float) * 8);
  memset(global_loaded_settings.l_angles, 0, sizeof(float) * 8);
  memset(global_loaded_settings.r_angles, 0, sizeof(float) * 8);
}

bool stick_scaling_capture_distances(a_data_s *in)
{
  // Get angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Adjust angles
  float sla = _stick_angle_adjust(la, 22.5f);
  float sra = _stick_angle_adjust(ra, 22.5f);

  int lo = _stick_get_octant(sla);
  int ro = _stick_get_octant(sra);

  // Get distance of input
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);


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

void stick_scaling_capture_center(a_data_s *in)
{
  _stick_l_center_x = in->lx;
  _stick_l_center_y = in->ly;

  _stick_r_center_x = in->rx;
  _stick_r_center_y = in->ry;
}

void stick_scaling_get_octant_axis(a_data_s *in, uint8_t *axis, uint8_t *octant)
{
  // Get angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get distance of inputs
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

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

void stick_scaling_get_octant_axis_offset(a_data_s *in, uint8_t *axis, uint8_t *octant)
{
  // Get angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  float sla = _stick_angle_adjust(la, 22.5);
  float sra = _stick_angle_adjust(ra, 22.5);

  // Get distance of inputs
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

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

// Captures a stick angle for octagon calibration
bool stick_scaling_capture_angle(a_data_s *in)
{
  // Get angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get angle adjusted to ensure we're adjusting the correct octant
  float sla = _stick_angle_adjust(la, 22.5f);
  float sra = _stick_angle_adjust(ra, 22.5f);

  // Get distance of inputs
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

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

void stick_scaling_process_data(a_data_s *in, a_data_s *out)
{
  // Get raw angles of input
  float la = _stick_get_angle(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float ra = _stick_get_angle(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

  // Get distance of input
  float ld = _stick_get_distance(in->lx, in->ly, _stick_l_center_x, _stick_l_center_y);
  float rd = _stick_get_distance(in->rx, in->ry, _stick_r_center_x, _stick_r_center_y);

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
