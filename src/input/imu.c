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
#include "utilities/crosscore_snapshot.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

// Include all IMU drivers
#include "drivers/imu/lsm6dsr.h"

#define IMU_CALIBRATE_CYCLES 2000
#define IMU_READ_RATE 1000

// Access IMU config union members macro
#define CH_A_GYRO_OFFSET(axis)    (imu_config->imu_a_gyro_offsets[axis])
#define CH_B_GYRO_OFFSET(axis)    (imu_config->imu_b_gyro_offsets[axis])
#define CH_A_ACCEL_CFG(axis)      (imu_config->imu_a_accel_config[axis])
#define CH_B_ACCEL_CFG(axis)      (imu_config->imu_b_accel_config[axis])

// Macro to access the offset for a given gyro axis channel
#define IMU_GYRO_OFFSET_X(channel)  (!channel ? CH_A_GYRO_OFFSET(0): CH_B_GYRO_OFFSET(0))
#define IMU_GYRO_OFFSET_Y(channel)  (!channel ? CH_A_GYRO_OFFSET(1) : CH_B_GYRO_OFFSET(1))
#define IMU_GYRO_OFFSET_Z(channel)  (!channel ? CH_A_GYRO_OFFSET(2) : CH_B_GYRO_OFFSET(2))

volatile bool _imu_do_update_quaternion = true;

SNAPSHOT_TYPE(imu, imu_data_s);
snapshot_imu_t _imu_snap;

SNAPSHOT_TYPE(quat, quaternion_s);
snapshot_quat_t _quat_snap;

imu_data_s _imu_data = {0};
quaternion_s _imu_quat_state = {.w = 1};

int16_t _imu_average_value(int16_t first, int16_t second)
{
  int total = ((int)first + (int)second) / 2;
  if (total > 32767)
    return 32767;
  if (total < -32768)
    return -32768;
  return total;
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

void _imu_quat_normalize(quaternion_s *data)
{
  float norm_inverse = 1.0f / sqrtf(data->x * data->x + data->y * data->y + data->z * data->z + data->w * data->w);
  data->x *= norm_inverse;
  data->y *= norm_inverse;
  data->z *= norm_inverse;
  data->w *= norm_inverse;
}

#define SCALE_FACTOR 2000.0f / INT16_MAX * M_PI / 180.0f / 1000000.0f

#define GYRO_SENS (2000.0f / 32768.0f)
void _imu_update_quaternion(imu_data_s *imu_data) {
    // Previous timestamp (in microseconds)
    static uint64_t prev_timestamp = 0;

    float dt = fabsf((float)imu_data->timestamp - (float)prev_timestamp);
    // Update the previous timestamp
    prev_timestamp = imu_data->timestamp;

    _imu_quat_state.timestamp = imu_data->timestamp/1000;

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

void _imu_read_standard(uint64_t timestamp)
{
  imu_data_s this_imu[3] = {0};

  #if defined(HOJA_IMU_CHAN_A_INIT) && defined(HOJA_IMU_CHAN_B_INIT)
  HOJA_IMU_CHAN_A_READ(&this_imu[0]);
  HOJA_IMU_CHAN_B_READ(&this_imu[1]);

  this_imu[0].gx -= IMU_GYRO_OFFSET_X(0);
  this_imu[0].gy -= IMU_GYRO_OFFSET_Y(0);
  this_imu[0].gz -= IMU_GYRO_OFFSET_Z(0);
  
  this_imu[1].gx -= IMU_GYRO_OFFSET_X(1);
  this_imu[1].gy -= IMU_GYRO_OFFSET_Y(1);
  this_imu[1].gz -= IMU_GYRO_OFFSET_Z(1);

  #elif defined(HOJA_IMU_CHAN_A_INIT)
  HOJA_IMU_CHAN_A_READ(&this_imu[0]);
  HOJA_IMU_CHAN_A_READ(&this_imu[1]);

  this_imu[0].gx -= IMU_GYRO_OFFSET_X(0);
  this_imu[0].gy -= IMU_GYRO_OFFSET_Y(0);
  this_imu[0].gz -= IMU_GYRO_OFFSET_Z(0);
  
  this_imu[1].gx -= IMU_GYRO_OFFSET_X(0);
  this_imu[1].gy -= IMU_GYRO_OFFSET_Y(0);
  this_imu[1].gz -= IMU_GYRO_OFFSET_Z(0);
  #endif

  // Average
  this_imu[2].ax = _imu_average_value(this_imu[0].ax, this_imu[1].ax);
  this_imu[2].ay = _imu_average_value(this_imu[0].ay, this_imu[1].ay);
  this_imu[2].az = _imu_average_value(this_imu[0].az, this_imu[1].az);

  this_imu[2].gx = _imu_average_value(this_imu[0].gx, this_imu[1].gx);
  this_imu[2].gy = _imu_average_value(this_imu[0].gy, this_imu[1].gy);
  this_imu[2].gz = _imu_average_value(this_imu[0].gz, this_imu[1].gz);
  this_imu[2].timestamp = timestamp;

  snapshot_imu_write(&_imu_snap, &this_imu[2]);

  if(_imu_do_update_quaternion)
  {
    static bool flipflop = false;
    flipflop = !flipflop;
    if(flipflop)
    {
      _imu_update_quaternion(&this_imu[2]);
      snapshot_quat_write(&_quat_snap, &_imu_quat_state);
    }
  }
}

// Access a pointer to the IMU data as it
void imu_access_safe(imu_data_s *out)
{
  snapshot_imu_read(&_imu_snap, out);
}

// Optional access Quaternion data (If available)
void imu_quaternion_access_safe(quaternion_s *out)
{
  snapshot_quat_read(&_quat_snap, out);
}

// Function we call when our IMU calibration is completed
webreport_cmd_confirm_t _calibrate_done_cb = NULL;
uint8_t _command = 0;

void _imu_calibrate_stop()
{
  hoja_set_notification_status(COLOR_BLACK);

  if(_calibrate_done_cb != NULL)
  { 
    _calibrate_done_cb(CFG_BLOCK_IMU, IMU_CMD_CALIBRATE_START, true, NULL, 0);
  }

  _calibrate_done_cb = NULL;
  _command = 0;
}

int _imu_calibrate_cycles_remaining = 0;
void _imu_calibrate_function(uint64_t timestamp)
{
  static interval_s calibration_interval = {0};

  if(!interval_run(timestamp, IMU_READ_RATE, &calibration_interval)) return;

  static int lx = 0;
  static int ly = 0;
  static int lz = 0;

  static int rx = 0;
  static int ry = 0;
  static int rz = 0;

  if(_imu_calibrate_cycles_remaining>0)
  {
    _imu_calibrate_cycles_remaining--;

    imu_data_s imu_calibration_read = {0};

    

    // Read IMU data
    #if defined(HOJA_IMU_CHAN_A_INIT)
    HOJA_IMU_CHAN_A_READ(&imu_calibration_read);
    lx += imu_calibration_read.gx;
    ly += imu_calibration_read.gy;
    lz += imu_calibration_read.gz;
    #endif

    #if defined(HOJA_IMU_CHAN_B_INIT)
    HOJA_IMU_CHAN_B_READ(&imu_calibration_read);
    rx += imu_calibration_read.gx;
    ry += imu_calibration_read.gy;
    rz += imu_calibration_read.gz;
    #endif
  }

  if(!_imu_calibrate_cycles_remaining)
  {
    #if defined(HOJA_IMU_CHAN_A_INIT)
    CH_A_GYRO_OFFSET(0) = (int8_t) (lx / IMU_CALIBRATE_CYCLES);
    CH_A_GYRO_OFFSET(1) = (int8_t) (ly / IMU_CALIBRATE_CYCLES);
    CH_A_GYRO_OFFSET(2) = (int8_t) (lz / IMU_CALIBRATE_CYCLES);
    lx = 0;
    ly = 0;
    lz = 0;
    #endif

    #if defined(HOJA_IMU_CHAN_B_INIT)
    CH_B_GYRO_OFFSET(0) = (int8_t) (rx / IMU_CALIBRATE_CYCLES);
    CH_B_GYRO_OFFSET(1) = (int8_t) (ry / IMU_CALIBRATE_CYCLES);
    CH_B_GYRO_OFFSET(2) = (int8_t) (rz / IMU_CALIBRATE_CYCLES);
    rx = 0;
    ry = 0;
    rz = 0;
    #endif
    _imu_calibrate_stop();
  }
}

void _imu_calibrate_start()
{
  hoja_set_notification_status(COLOR_YELLOW);
  _imu_calibrate_cycles_remaining = IMU_CALIBRATE_CYCLES;
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
void imu_task(uint64_t timestamp)
{
  static interval_s _imu_read_interval = {0};

  if(interval_run(timestamp, IMU_READ_RATE, &_imu_read_interval))
  {
    // Jump into appropriate IMU task if it's defined
    if(_imu_calibrate_cycles_remaining)
      _imu_calibrate_function(timestamp);
    else
      _imu_read_standard(timestamp);
  }
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
  #endif 

  #if defined(HOJA_IMU_CHAN_B_INIT)
  HOJA_IMU_CHAN_B_INIT();
  #endif

  return true;
}