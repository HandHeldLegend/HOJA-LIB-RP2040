#include "input/imu.h"

#include "hal/mutex_hal.h"

#include "utilities/interval.h"

#define IMU_READ_RATE 1000 // 500KHz 
#define IMU_CALIBRATE_CYCLES 16000

imu_data_s _imu_buffer_a = {0};
imu_data_s _imu_buffer_b = {0};

imu_data_s _imu_buffer_avg = {0};

#define IMU_FIFO_COUNT 3
#define IMU_FIFO_IDX_MAX (IMU_FIFO_COUNT-1)
int _imu_fifo_idx = 0;
imu_data_s _imu_fifo[IMU_FIFO_COUNT];

auto_init_mutex(_imu_mutex);
uint32_t _imu_owner_0;
uint32_t _imu_owner_1;

quaternion_s _imu_quat_state = {.w = 1};
#define GYRO_SENS (2000.0f / 32768.0f)

MUTEX_HAL_INIT(_imu_mutex);
void _imu_safe_enter()
{
    MUTEX_HAL_ENTER_BLOCKING(&_imu_mutex);
}

void _imu_exit()
{
    MUTEX_HAL_EXIT(&_imu_mutex);
}

void imu_access(imu_data_s *out, imu_access_t type)
{
    _imu_safe_enter();
    // Only one type for now
    _imu_exit();
}

void imu_task(uint32_t timestamp)
{

}

void _imu_read(imu_data_s *a, imu_data_s *b)
{
  #ifdef HOJA_IMU_CHAN_A_READ
    HOJA_IMU_CHAN_A_READ(a);
  #endif

  #ifdef HOJA_IMU_CHAN_B_READ
    HOJA_IMU_CHAN_B_READ(b);
  #endif
}

void imu_get_quat(quaternion_s *out)
{
  out = &_imu_quat_state;
}

void imu_pack_quat(mode_2_s *out)
{
  static uint32_t last_time;
  uint32_t time;
  static uint32_t accumulated_delta = 0;
  static uint32_t q_timestamp = 0;

  out->mode = 2;

  // Determine maximum quat component
  uint8_t max_index = 0;
  for(uint8_t i = 1; i < 4; i++)
  {
    if(_imu_quat_state.raw[i] > fabsf(_imu_quat_state.raw[max_index]))
      max_index = i;
  }

  out->max_index = max_index;

  int quaternion_30bit_components[3];

  // Exclude the max_index component from the component list, invert sign of the remaining components if it was negative. Scales the final result to a 30 bit fixed precision format where 0x40000000 is 1.0
  for (int i = 0; i < 3; ++i) {
      quaternion_30bit_components[i] = _imu_quat_state.raw[(max_index + i + 1) & 3] * 0x40000000 * (_imu_quat_state.raw[max_index] < 0 ? -1 : 1);
  }

  // Insert into the last sample components, do bit operations to account for split data
  out->last_sample_0 = quaternion_30bit_components[0] >> 10;

  out->last_sample_1l = ((quaternion_30bit_components[1] >> 10) & 0x7F);
  out->last_sample_1h = ((quaternion_30bit_components[1] >> 10) & 0x1FFF80) >> 7;

  out->last_sample_2l = ((quaternion_30bit_components[2] >> 10) & 0x3);
  out->last_sample_2h = ((quaternion_30bit_components[2] >> 10) & 0x1FFFFC) >> 2;

  // We only store one sample, so all deltas are 0
  out->delta_last_first_0 = 0;
  out->delta_last_first_1 = 0;
  out->delta_last_first_2l = 0;
  out->delta_last_first_2h = 0;
  out->delta_mid_avg_0 = 0;
  out->delta_mid_avg_1 = 0;
  out->delta_mid_avg_2 = 0;

  // Timestamps handling is still a bit unclear, these are the values that motion_data in no drifting 
  out->timestamp_start_l = q_timestamp & 0x1;
  out->timestamp_start_h = (q_timestamp  >> 1) & 0x3FF;
  out->timestamp_count = 3;

  out->accel_0.x = _imu_quat_state.ax;
  out->accel_0.y = _imu_quat_state.ay;
  out->accel_0.z = _imu_quat_state.az;

  time = hoja_get_timestamp();
  uint32_t delta = 0;
  uint32_t whole = 0;
  // Increment only by changed time
  if (time < last_time)
  {
    delta = (0xFFFFFFFF - last_time) + time;
  }
  else if (time >= last_time)
  {
    delta = time - last_time;
  }

  last_time = time;

  accumulated_delta += delta;

  if(accumulated_delta > 1000)
  {
    whole = accumulated_delta;
    accumulated_delta %= 1000;
    whole -= accumulated_delta;
    whole /= 1000; // Convert to ms
    // Increment for the next cycle
    q_timestamp += whole;
    q_timestamp %= 0x7FF;
  }
}

void _imu_rotate_quaternion(quaternion_s *first, quaternion_s *second) {
    float w = first->w * second->w - first->x * second->x - first->y * second->y - first->z * second->z;
    float x = first->w * second->x + first->x * second->w + first->y * second->z - first->z * second->y;
    float y = first->w * second->y - first->x * second->z + first->y * second->w + first->z * second->x;
    float z = first->w * second->z + first->x * second->y - first->y * second->x + first->z * second->w;
    
    first->w = w;
    first->x = x;
    first->y = y;
    first->z = z;
}

#define SCALE_FACTOR 2000.0f / INT16_MAX * M_PI / 180.0f / 1000000.0f

void _imu_quat_normalize(quaternion_s *data)
{
  float norm_inverse = 1.0f / sqrtf(data->x * data->x + data->y * data->y + data->z * data->z + data->w * data->w);
  data->x *= norm_inverse;
  data->y *= norm_inverse;
  data->z *= norm_inverse;
  data->w *= norm_inverse;
}

void _imu_update_quaternion(imu_data_s *imu_data, uint32_t timestamp) {
    // Previous timestamp (in microseconds)
    static uint32_t prev_timestamp = 0;

    float dt = fabsf((float)timestamp - (float)prev_timestamp);
    // Update the previous timestamp
    prev_timestamp = timestamp;

    // GZ is TURNING left/right (steering axis)
    // GX is TILTING up/down (aim up/down)
    // GY is TILTING left/right

    // Convert gyro readings to radians/second
    float angle_x = (float)imu_data->gy * SCALE_FACTOR * dt; // GY
    float angle_y = (float)imu_data->gx * SCALE_FACTOR * dt; // GX
    float angle_z = (float)imu_data->gz * SCALE_FACTOR * dt; // GZ

    // Euler to quaternion (in a custom Nintendo way)
    double norm_squared = angle_x * angle_x + angle_y * angle_y + angle_z * angle_z;
    double first_formula = norm_squared * norm_squared / 3840.0f - norm_squared / 48 + 0.5;
    double second_formula = norm_squared * norm_squared / 384.0f - norm_squared / 8 + 1;

    quaternion_s newstate = {
      .x = angle_x * first_formula,
      .y = angle_y * first_formula,
      .z = angle_z * first_formula,
      .w = second_formula
    };

    _imu_rotate_quaternion(&_imu_quat_state, &newstate);

    _imu_quat_normalize(&_imu_quat_state);

    _imu_quat_state.ax = imu_data->ax;
    _imu_quat_state.ay = imu_data->ay;
    _imu_quat_state.az = imu_data->az;

    
}

// Add data to our FIFO
void imu_fifo_push(imu_data_s *imu_data, uint32_t timestamp)
{
    while(!mutex_try_enter(&_imu_mutex, &_imu_owner_0)) {}

    int _i = (_imu_fifo_idx+1) % IMU_FIFO_COUNT;

    _imu_fifo[_i].ax = imu_data->ax;
    _imu_fifo[_i].ay = imu_data->ay;
    _imu_fifo[_i].az = imu_data->az;

    _imu_fifo[_i].gx = imu_data->gx;
    _imu_fifo[_i].gy = imu_data->gy;
    _imu_fifo[_i].gz = imu_data->gz;

    _imu_fifo_idx = _i;

    _imu_update_quaternion(imu_data, timestamp);

    mutex_exit(&_imu_mutex);
}

imu_data_s* imu_fifo_last()
{
  while(!mutex_try_enter(&_imu_mutex, &_imu_owner_1)) {}
  int idx = _imu_fifo_idx;
  mutex_exit(&_imu_mutex);
  return &(_imu_fifo[idx]);
}

uint8_t _imu_read_idx = 0;
bool _imu_enabled = false;
bool _imu_calibrate = false;
int _imu_calibrate_cycles_remaining = 0;

int16_t _imu_average_value(int16_t first, int16_t second)
{
  int total = ((int)first + (int)second) / 2;
  if (total > 32767)
    return 32767;
  if (total < -32768)
    return -32768;
  return total;
}

bool _imu_calibrate_init = false;
void imu_calibrate_start()
{
  // Change leds to yellow
  rgb_s c = {
      .r = 0xFF,
      .g = 0xFF,
      .b = 0,
  };

  rgb_flash(c.color, -1);

  // Reset offsets to 0
  memset(global_loaded_settings.imu_0_offsets, 0x00, sizeof(int8_t) * 6);
  memset(global_loaded_settings.imu_1_offsets, 0x00, sizeof(int8_t) * 6);

  _imu_calibrate = true;
  _imu_calibrate_cycles_remaining = IMU_CALIBRATE_CYCLES;
  _imu_calibrate_init = false;

  // Read IMU and store first reading
  _imu_read(&_imu_buffer_a, &_imu_buffer_b);

  global_loaded_settings.imu_0_offsets[0] = _imu_buffer_a.ax;
  global_loaded_settings.imu_0_offsets[1] = _imu_buffer_a.ay;
  global_loaded_settings.imu_0_offsets[2] = _imu_buffer_a.az;

  global_loaded_settings.imu_0_offsets[3] = _imu_buffer_a.gx;
  global_loaded_settings.imu_0_offsets[4] = _imu_buffer_a.gy;
  global_loaded_settings.imu_0_offsets[5] = _imu_buffer_a.gz;

  global_loaded_settings.imu_1_offsets[0] = _imu_buffer_b.ax;
  global_loaded_settings.imu_1_offsets[1] = _imu_buffer_b.ay;
  global_loaded_settings.imu_1_offsets[2] = _imu_buffer_b.az;

  global_loaded_settings.imu_1_offsets[3] = _imu_buffer_b.gx;
  global_loaded_settings.imu_1_offsets[4] = _imu_buffer_b.gy;
  global_loaded_settings.imu_1_offsets[5] = _imu_buffer_b.gz;
}

void imu_calibrate_stop()
{

  rgb_init(global_loaded_settings.rgb_mode, BRIGHTNESS_RELOAD);

  settings_save_webindicate();
  settings_save_from_core0();

  _imu_calibrate = false;
  
}

void _imu_calibrate_task()
{
  #if (HOJA_IMU_DRIVER_ENABLED==1)
    // Read IMU
    // cb_hoja_read_imu(&_imu_buffer_a, &_imu_buffer_b);
    _imu_read(&_imu_buffer_a, &_imu_buffer_b);

    if(!_imu_calibrate_init)
    {
      global_loaded_settings.imu_0_offsets[0] = _imu_buffer_a.ax;
      global_loaded_settings.imu_0_offsets[1] = _imu_buffer_a.ay;
      global_loaded_settings.imu_0_offsets[2] = _imu_buffer_a.az;
      global_loaded_settings.imu_0_offsets[3] = _imu_buffer_a.gx;
      global_loaded_settings.imu_0_offsets[4] = _imu_buffer_a.gy;
      global_loaded_settings.imu_0_offsets[5] = _imu_buffer_a.gz;

      global_loaded_settings.imu_1_offsets[0] = _imu_buffer_b.ax;
      global_loaded_settings.imu_1_offsets[1] = _imu_buffer_b.ay;
      global_loaded_settings.imu_1_offsets[2] = _imu_buffer_b.az;
      global_loaded_settings.imu_1_offsets[3] = _imu_buffer_b.gx;
      global_loaded_settings.imu_1_offsets[4] = _imu_buffer_b.gy;
      global_loaded_settings.imu_1_offsets[5] = _imu_buffer_b.gz;
      _imu_calibrate_init = true;
      return;
    }

    global_loaded_settings.imu_0_offsets[0] = _imu_average_value(global_loaded_settings.imu_0_offsets[0], _imu_buffer_a.ax);
    global_loaded_settings.imu_0_offsets[1] = _imu_average_value(global_loaded_settings.imu_0_offsets[1], _imu_buffer_a.ay);
    global_loaded_settings.imu_0_offsets[2] = _imu_average_value(global_loaded_settings.imu_0_offsets[2], _imu_buffer_a.az);
    global_loaded_settings.imu_0_offsets[3] = _imu_average_value(global_loaded_settings.imu_0_offsets[3], _imu_buffer_a.gx);
    global_loaded_settings.imu_0_offsets[4] = _imu_average_value(global_loaded_settings.imu_0_offsets[4], _imu_buffer_a.gy);
    global_loaded_settings.imu_0_offsets[5] = _imu_average_value(global_loaded_settings.imu_0_offsets[5], _imu_buffer_a.gz);

    global_loaded_settings.imu_1_offsets[0] = _imu_average_value(global_loaded_settings.imu_1_offsets[0], _imu_buffer_b.ax);
    global_loaded_settings.imu_1_offsets[1] = _imu_average_value(global_loaded_settings.imu_1_offsets[1], _imu_buffer_b.ay);
    global_loaded_settings.imu_1_offsets[2] = _imu_average_value(global_loaded_settings.imu_1_offsets[2], _imu_buffer_b.az);
    global_loaded_settings.imu_1_offsets[3] = _imu_average_value(global_loaded_settings.imu_1_offsets[3], _imu_buffer_b.gx);
    global_loaded_settings.imu_1_offsets[4] = _imu_average_value(global_loaded_settings.imu_1_offsets[4], _imu_buffer_b.gy);
    global_loaded_settings.imu_1_offsets[5] = _imu_average_value(global_loaded_settings.imu_1_offsets[5], _imu_buffer_b.gz);

    _imu_buffer_a.retrieved = false;
    _imu_buffer_b.retrieved = false;

    _imu_calibrate_cycles_remaining--;
    if (!_imu_calibrate_cycles_remaining)
    {
      imu_calibrate_stop();
    }
  #endif
}

void imu_set_enabled(bool enable)
{
  _imu_enabled = enable;
}

void imu_task(uint32_t timestamp)
{
  #if (HOJA_IMU_DRIVER_ENABLED==1)

  static interval_s interval = {0};

  if (_imu_calibrate)
  {
    _imu_calibrate_task();
    return;
  }

  if (!_imu_enabled)
    return;

  if (interval_run(timestamp, 1000, &interval))
  {
    _imu_read(&_imu_buffer_a, &_imu_buffer_b);

    // If we received two buffers, average A and B
    if (_imu_buffer_b.retrieved && _imu_buffer_a.retrieved)
    {

      _imu_buffer_avg.ax = _imu_average_value(_imu_buffer_a.ax-global_loaded_settings.imu_0_offsets[0], _imu_buffer_b.ax-global_loaded_settings.imu_1_offsets[0]);
      _imu_buffer_avg.ay = _imu_average_value(_imu_buffer_a.ay-global_loaded_settings.imu_0_offsets[1] , _imu_buffer_b.ay-global_loaded_settings.imu_1_offsets[1]);
      _imu_buffer_avg.az = _imu_average_value(_imu_buffer_a.az-global_loaded_settings.imu_0_offsets[2], _imu_buffer_b.az-global_loaded_settings.imu_1_offsets[2]);

      _imu_buffer_avg.gx = _imu_average_value(_imu_buffer_a.gx-global_loaded_settings.imu_0_offsets[3], _imu_buffer_b.gx-global_loaded_settings.imu_1_offsets[3]);
      _imu_buffer_avg.gy = _imu_average_value(_imu_buffer_a.gy-global_loaded_settings.imu_0_offsets[4], _imu_buffer_b.gy-global_loaded_settings.imu_1_offsets[4]);
      _imu_buffer_avg.gz = _imu_average_value(_imu_buffer_a.gz-global_loaded_settings.imu_0_offsets[5], _imu_buffer_b.gz-global_loaded_settings.imu_1_offsets[5]);

      imu_fifo_push(&_imu_buffer_avg, timestamp);
    }
    else if (_imu_buffer_a.retrieved)
    {
      imu_fifo_push(&_imu_buffer_a, timestamp);
    }

    _imu_buffer_a.retrieved = false;
    _imu_buffer_b.retrieved = false;
  }

  #endif
}
