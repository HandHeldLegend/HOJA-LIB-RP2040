#include "input/imu.h"
#include "hoja.h"

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"

#include "utilities/callback.h"

#include "utilities/interval.h"
#include "devices_shared_types.h"
#include "settings_shared_types.h"

#include "usb/webusb.h"

#include "utilities/settings.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

// Include all IMU drivers
#include "drivers/imu/lsm6dsr.h"

#define IMU_POLLING_INTERVAL 2500
#define IMU_CALIBRATE_CYCLES 4000

// Define data that is safe to access
// from any core at any time
MUTEX_HAL_INIT(_imu_mutex);
imu_data_s    _safe_imu_data = {0};
quaternion_s  _safe_quaternion_data = {0};

// Raw IMU buffers for channel A and B
imu_data_s _imu_buffer_a = {0};
imu_data_s _imu_buffer_b = {0};

// Average IMU buffer containing both A and B data averaged together
imu_data_s _imu_buffer_avg = {0};

// Processing task which we use to handle the newly read IMU data
callback_t _imu_process_task = NULL;

// Access IMU config union members macro
#define CH_A_GYRO_OFFSET(axis)    (imu_config->imu_a_gyro_offsets[axis])
#define CH_B_GYRO_OFFSET(axis)    (imu_config->imu_b_gyro_offsets[axis])
#define CH_A_ACCEL_CFG(axis)      (imu_config->imu_a_accel_config[axis])
#define CH_B_ACCEL_CFG(axis)      (imu_config->imu_b_accel_config[axis])

// Macro to access the offset for a given gyro axis channel
#define IMU_GYRO_OFFSET_X(channel)  (!channel ? CH_A_GYRO_OFFSET(0): CH_B_GYRO_OFFSET(0))
#define IMU_GYRO_OFFSET_Y(channel)  (!channel ? CH_A_GYRO_OFFSET(1) : CH_B_GYRO_OFFSET(1))
#define IMU_GYRO_OFFSET_Z(channel)  (!channel ? CH_A_GYRO_OFFSET(2) : CH_B_GYRO_OFFSET(2))

#define IMU_FIFO_COUNT 4
#define IMU_FIFO_IDX_MAX (IMU_FIFO_COUNT-1)
int _imu_fifo_idx = 0;
imu_data_s _imu_fifo[IMU_FIFO_COUNT];
volatile imu_data_s _imu_data_safe = {0};

quaternion_s _imu_quat_state = {.w = 1};
#define GYRO_SENS (2000.0f / 32768.0f)


void _imu_blocking_enter()
{
  MUTEX_HAL_ENTER_BLOCKING(&_imu_mutex);
}

uint32_t _imu_mutex_owner = 0;
bool _imu_try_enter()
{
  if(MUTEX_HAL_ENTER_TRY(&_imu_mutex, &_imu_mutex_owner))
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

void imu_access_safe(imu_data_s *out)
{
  //if(_imu_try_enter())
  //{
  //  memcpy(out, &_safe_imu_data, sizeof(imu_data_s));
  //  _imu_exit();
  //}

  memcpy(out, &_imu_data_safe, sizeof(imu_data_s));
}

// Optional access Quaternion data (If available)
void imu_quaternion_access_safe(quaternion_s *out)
{
  memcpy(out, &_safe_quaternion_data, sizeof(quaternion_s));
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
void _imu_fifo_push(imu_data_s *imu_data)
{ 
  static uint32_t timestamp = 0;

  int _i = (_imu_fifo_idx+1) % IMU_FIFO_COUNT;

  _imu_data_safe = *imu_data;

  _imu_fifo_idx = _i;

  // Update timestamp since we blocked for unknown time
  // We need accurate timings for our quaternion calculation/updates
  timestamp = sys_hal_time_us();
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

void _imu_std_function()
{
  // Apply offsets
  _imu_buffer_a.gx -= IMU_GYRO_OFFSET_X(0);
  _imu_buffer_a.gy -= IMU_GYRO_OFFSET_Y(0);
  _imu_buffer_a.gz -= IMU_GYRO_OFFSET_Z(0);
  _imu_buffer_b.gx -= IMU_GYRO_OFFSET_X(1);
  _imu_buffer_b.gy -= IMU_GYRO_OFFSET_Y(1);
  _imu_buffer_b.gz -= IMU_GYRO_OFFSET_Z(1);

  // If we received two buffers, average A and B
  if (_imu_buffer_b.retrieved && _imu_buffer_a.retrieved)
  {

    _imu_buffer_avg.ax = _imu_average_value(_imu_buffer_a.ax, _imu_buffer_b.ax);
    _imu_buffer_avg.ay = _imu_average_value(_imu_buffer_a.ay, _imu_buffer_b.ay);
    _imu_buffer_avg.az = _imu_average_value(_imu_buffer_a.az, _imu_buffer_b.az);

    _imu_buffer_avg.gx = _imu_average_value(_imu_buffer_a.gx, _imu_buffer_b.gx);
    _imu_buffer_avg.gy = _imu_average_value(_imu_buffer_a.gy, _imu_buffer_b.gy);
    _imu_buffer_avg.gz = _imu_average_value(_imu_buffer_a.gz, _imu_buffer_b.gz);

    _imu_fifo_push(&_imu_buffer_avg);
  }
  else if (_imu_buffer_a.retrieved)
  {
    _imu_fifo_push(&_imu_buffer_a);
  }

  _imu_buffer_a.retrieved = false;
  _imu_buffer_b.retrieved = false;
}

// Function we call when our IMU calibration is completed
webreport_cmd_confirm_t _calibrate_done_cb = NULL;
uint8_t _command = 0;

void _imu_calibrate_stop()
{
  hoja_set_notification_status(COLOR_BLACK);
  _imu_process_task = _imu_std_function;

  if(_calibrate_done_cb != NULL)
  { 
    _calibrate_done_cb(CFG_BLOCK_IMU, IMU_CMD_CALIBRATE_START, true, NULL, 0);
  }

  _calibrate_done_cb = NULL;
  _command = 0;
}

int _imu_calibrate_cycles_remaining = 0;
void _imu_calibrate_function()
{
  if(_imu_calibrate_cycles_remaining>0)
  {
    _imu_calibrate_cycles_remaining--;

    // Read IMU data
    _imu_read(&_imu_buffer_a, &_imu_buffer_b);

    #if defined(HOJA_IMU_CHAN_A_READ)
    CH_A_GYRO_OFFSET(0) =  _imu_average_value(CH_A_GYRO_OFFSET(0), _imu_buffer_a.gx);
    CH_A_GYRO_OFFSET(1) =  _imu_average_value(CH_A_GYRO_OFFSET(1), _imu_buffer_a.gy);
    CH_A_GYRO_OFFSET(2) =  _imu_average_value(CH_A_GYRO_OFFSET(2), _imu_buffer_a.gz);
    _imu_buffer_a.retrieved = false;
    #if defined(HOJA_IMU_CHAN_B_READ)
    CH_B_GYRO_OFFSET(0) =  _imu_average_value(CH_B_GYRO_OFFSET(0), _imu_buffer_b.gx);
    CH_B_GYRO_OFFSET(1) =  _imu_average_value(CH_B_GYRO_OFFSET(1), _imu_buffer_b.gy);
    CH_B_GYRO_OFFSET(2) =  _imu_average_value(CH_B_GYRO_OFFSET(2), _imu_buffer_b.gz);
    _imu_buffer_b.retrieved = false;
    #endif
    #endif
  }
  else
  {
    _imu_calibrate_stop();
  }
}

void _imu_calibrate_start()
{
  hoja_set_notification_status(COLOR_YELLOW);
  _imu_calibrate_cycles_remaining = IMU_CALIBRATE_CYCLES;
  _imu_process_task = _imu_calibrate_function;
}

// IMU module command handler
void imu_config_cmd(imu_cmd_t cmd, webreport_cmd_confirm_t cb)
{
  switch(cmd)
  {
    default:
    return;
    break;

    case IMU_CMD_CALIBRATE_START:
      _command = cmd;
      _calibrate_done_cb = cb;
      _imu_calibrate_start();
    // We don't send a callback until the calibration is done.
    break;
  }
}

// IMU module operational task
void imu_task(uint32_t timestamp)
{
  static interval_s interval = {0};

  if (interval_run(timestamp, IMU_POLLING_INTERVAL, &interval))
  {
    if(imu_config->imu_disabled==1)
    {
      memset(&_imu_buffer_a, 0, sizeof(imu_data_s));
      memset(&_imu_buffer_b, 0, sizeof(imu_data_s));
      _imu_buffer_a.retrieved = true;
      _imu_buffer_b.retrieved = true;
    }
    else
    {
      // Read IMU data
      _imu_read(&_imu_buffer_a, &_imu_buffer_b);
    }

    // Jump into appropriate IMU task if it's defined
    if(_imu_process_task)
      _imu_process_task();
    else
      _imu_process_task = _imu_std_function;
  }

  memcpy(&_safe_quaternion_data, &_imu_quat_state, sizeof(quaternion_s));
}

// IMU module initialization function
bool imu_init()
{
  // Verify or default IMU
  if(imu_config->imu_config_version != CFG_BLOCK_IMU_VERSION)
  {
    imu_config->imu_config_version = CFG_BLOCK_IMU_VERSION;
    imu_config->imu_disabled = 0;
    memset(&imu_config->imu_a_gyro_offsets, 0, 3);
    memset(&imu_config->imu_a_accel_config, 0, 3);
    memset(&imu_config->imu_b_gyro_offsets, 0, 3);
    memset(&imu_config->imu_b_accel_config, 0, 3);
  }

  #if defined(HOJA_IMU_CHAN_A_INIT)
  HOJA_IMU_CHAN_A_INIT();
  _imu_process_task = _imu_std_function;
  #endif 

  #if defined(HOJA_IMU_CHAN_B_INIT)
  HOJA_IMU_CHAN_B_INIT()
  #endif

  return true;
}