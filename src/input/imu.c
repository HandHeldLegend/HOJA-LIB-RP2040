#include "input/imu.h"

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"

#include "utilities/callback.h"

#include "utilities/interval.h"
#include "devices_shared_types.h"
#include "settings_shared_types.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

// Include all IMU drivers
#include "drivers/imu/lsm6dsr.h"

#define IMU_READ_RATE 1000 // 500KHz 
#define IMU_CALIBRATE_CYCLES 16000

// Raw IMU buffers for channel A and B
imu_data_s _imu_buffer_a = {0};
imu_data_s _imu_buffer_b = {0};

// Average IMU buffer containing both A and B data averaged together
imu_data_s _imu_buffer_avg = {0};

// Data that contains the config block for the IMU
imu_config_u _imu_config = {0};

// Processing task which we use to handle the newly read IMU data
time_callback_t _imu_process_task = NULL;

// Access IMU config union members macro
#define CH_A_GYRO_CFG(axis) (_imu_config.imu_a_gyro_config[axis])
#define CH_B_GYRO_CFG(axis) (_imu_config.imu_b_gyro_config[axis])
#define CH_A_ACCEL_CFG(axis) (_imu_config.imu_a_accel_config[axis])
#define CH_B_ACCEL_CFG(axis) (_imu_config.imu_b_accel_config[axis])

// Macro to retrieve IMU offset based on set bit flag
#define IMU_OFFSET_GET(cfg) ( (cfg & 0b1000000) ? ((int) -(cfg & 0b111111)) : ((int) cfg & 0b111111) )

// Macro to access the offset for a given gyro axis channel
#define IMU_GYRO_OFFSET_X(channel)  (!channel ? IMU_OFFSET_GET(CH_A_GYRO_CFG(0)) : IMU_OFFSET_GET(CH_B_GYRO_CFG(0)))
#define IMU_GYRO_OFFSET_Y(channel)  (!channel ? IMU_OFFSET_GET(CH_A_GYRO_CFG(1)) : IMU_OFFSET_GET(CH_B_GYRO_CFG(1)))
#define IMU_GYRO_OFFSET_Z(channel)  (!channel ? IMU_OFFSET_GET(CH_A_GYRO_CFG(2)) : IMU_OFFSET_GET(CH_B_GYRO_CFG(2)))

#define IMU_FIFO_COUNT 4
#define IMU_FIFO_IDX_MAX (IMU_FIFO_COUNT-1)
int _imu_fifo_idx = 0;
imu_data_s _imu_fifo[IMU_FIFO_COUNT];

quaternion_s _imu_quat_state = {.w = 1};
#define GYRO_SENS (2000.0f / 32768.0f)

MUTEX_HAL_INIT(_imu_mutex);
void _imu_blocking_enter()
{
  MUTEX_HAL_ENTER_BLOCKING(&_imu_mutex);
}

bool _imu_try_enter()
{
  if(MUTEX_HAL_ENTER_TIMEOUT_US(&_imu_mutex, 10))
  {
    return true;
  }
  return false;
}

void _imu_exit()
{
  MUTEX_HAL_EXIT(&_imu_mutex);
}

imu_data_s* _imu_fifo_last()
{
  int idx = _imu_fifo_idx;
  return &(_imu_fifo[idx]);
}

// Optional access IMU data (If available)
bool imu_access_try(imu_data_s *out)
{
  if(_imu_try_enter())
  {
    imu_data_s* tmp = _imu_fifo_last();
    memcpy(out, tmp, sizeof(imu_data_s));
    _imu_exit();
    return true;
  }
  return false;
}

// Access IMU data and block for it
void imu_access_block(imu_data_s *out)
{
  _imu_blocking_enter();
  imu_data_s* tmp = _imu_fifo_last();
  memcpy(out, tmp, sizeof(imu_data_s));
  _imu_exit();
}

// Optional access Quaternion data (If available)
bool imu_quaternion_access_try(quaternion_s *out)
{
  if(_imu_try_enter())
  {
    memcpy(out, &_imu_quat_state, sizeof(quaternion_s));
    _imu_exit();
    return true;
  }
  return false;
}

// Access Quaternion data and block for it
void imu_quaternion_access_block(quaternion_s *out)
{
  _imu_blocking_enter();
  memcpy(out, &_imu_quat_state, sizeof(quaternion_s));
  _imu_exit();
}

// Read IMU hardware from driver (if defined)
void _imu_read(imu_data_s *a, imu_data_s *b)
{
  #ifdef HOJA_IMU_CHAN_A_READ
    HOJA_IMU_CHAN_A_READ(a);
  #endif

  #ifdef HOJA_IMU_CHAN_B_READ
    HOJA_IMU_CHAN_B_READ(b);
  #endif
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
void _imu_fifo_push(imu_data_s *imu_data, uint32_t timestamp)
{ 
  int _i = (_imu_fifo_idx+1) % IMU_FIFO_COUNT;

  _imu_fifo[_i].ax = imu_data->ax;
  _imu_fifo[_i].ay = imu_data->ay;
  _imu_fifo[_i].az = imu_data->az;

  _imu_fifo[_i].gx = imu_data->gx;
  _imu_fifo[_i].gy = imu_data->gy;
  _imu_fifo[_i].gz = imu_data->gz;

  _imu_fifo_idx = _i;

  _imu_update_quaternion(imu_data, timestamp);
}

int16_t _imu_average_value(int16_t first, int16_t second)
{
  int total = ((int)first + (int)second) / 2;
  if (total > 32767)
    return 32767;
  if (total < -32768)
    return -32768;
  return total;
}

void _imu_std_function(uint32_t timestamp)
{
  // Apply offsets
  _imu_buffer_a.gx += IMU_GYRO_OFFSET_X(0);
  _imu_buffer_a.gy += IMU_GYRO_OFFSET_Y(0);
  _imu_buffer_a.gz += IMU_GYRO_OFFSET_Z(0);

  _imu_buffer_b.gx += IMU_GYRO_OFFSET_X(1);
  _imu_buffer_b.gy += IMU_GYRO_OFFSET_Y(1);
  _imu_buffer_b.gz += IMU_GYRO_OFFSET_Z(1);

  // If we received two buffers, average A and B
  if (_imu_buffer_b.retrieved && _imu_buffer_a.retrieved)
  {

    _imu_buffer_avg.ax = _imu_average_value(_imu_buffer_a.ax, _imu_buffer_b.ax);
    _imu_buffer_avg.ay = _imu_average_value(_imu_buffer_a.ay, _imu_buffer_b.ay);
    _imu_buffer_avg.az = _imu_average_value(_imu_buffer_a.az, _imu_buffer_b.az);

    _imu_buffer_avg.gx = _imu_average_value(_imu_buffer_a.gx, _imu_buffer_b.gx);
    _imu_buffer_avg.gy = _imu_average_value(_imu_buffer_a.gy, _imu_buffer_b.gy);
    _imu_buffer_avg.gz = _imu_average_value(_imu_buffer_a.gz, _imu_buffer_b.gz);

    _imu_fifo_push(&_imu_buffer_avg, timestamp);
  }
  else if (_imu_buffer_a.retrieved)
  {
    _imu_fifo_push(&_imu_buffer_a, timestamp);
  }

  _imu_buffer_a.retrieved = false;
  _imu_buffer_b.retrieved = false;
}

setting_callback_t _calibrate_done_cb = NULL;
void _imu_calibrate_stop()
{
  _imu_blocking_enter();
  _imu_process_task = _imu_std_function;
  // Send success code
  const uint8_t done[1] = {1};

  if(_calibrate_done_cb != NULL)
    _calibrate_done_cb(done, 1);

  _imu_exit();
}

int _imu_calibrate_cycles_remaining = 0;
void _imu_calibrate_function(uint32_t timestamp)
{
  if(_imu_calibrate_cycles_remaining>0)
  {
    _imu_calibrate_cycles_remaining--;

    // TODO implement IMU calibration method
  }
  else
  {
    _imu_calibrate_stop();
  }
}


void _imu_calibrate_start()
{
  _imu_blocking_enter();
  _imu_calibrate_cycles_remaining = IMU_CALIBRATE_CYCLES;
  _imu_process_task = _imu_calibrate_function;
  _imu_exit();
}


void imu_command_handler(imu_cmd_t cmd, setting_callback_t cb)
{
  const uint8_t cb_dat[1] = {0};

  switch(cmd)
  {
    default:
    cb(cb_dat, 0); // Do nothing
    break;

    case IMU_CMD_CALIBRATE:
    // Start calibration
    _calibrate_done_cb = cb;
    _imu_calibrate_start();

    // We don't send a callback until the calibration is done.
    break;
  }
}

void imu_task(uint32_t timestamp)
{
  static interval_s interval = {0};

  if (interval_run(timestamp, 1000, &interval))
  {
    _imu_blocking_enter();

    // Update timestamp since we blocked for unknown time
    // We need accurate timings for our quaternion calculation/updates
    timestamp = sys_hal_time_us();

    // Read IMU data
    _imu_read(&_imu_buffer_a, &_imu_buffer_b);

    // Jump into appropriate IMU task if it's defined
    if(_imu_process_task!=NULL)
      _imu_process_task(timestamp);
    else
      _imu_process_task = _imu_std_function;

    _imu_exit();
  }
}

bool imu_init()
{
  #if defined(HOJA_IMU_CHAN_A_INIT)
  HOJA_IMU_CHAN_A_INIT();
  _imu_process_task = _imu_std_function;
  #endif 

  #if defined(HOJA_IMU_CHAN_B_INIT)
  HOJA_IMU_CHAN_B_INIT()
  #endif
}