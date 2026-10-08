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
#include "utilities/crosscore_utils.h"
#include "devices/rgb.h"

#include "ns_lib_motion.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

typedef void (*imu_fn_t)(uint64_t);
imu_fn_t _imu_fn = NULL;

// Weak driver contract defaults. A board with no IMU driver selected links
// these, making every IMU call a safe no-op. The selected driver (compiled in
// by the HOJA_IMU_DRIVER gate) overrides them.
__attribute__((weak)) uint8_t imu_driver_channel_count(void) { return 0; }
__attribute__((weak)) bool imu_driver_init(void) { return false; }
__attribute__((weak)) bool imu_driver_read(uint8_t channel, imu_data_s *out)
{
  (void)channel;
  (void)out;
  return false;
}
__attribute__((weak)) const char *imu_driver_part_code(void) { return NULL; }

#define IMU_CALIBRATE_CYCLES 2000
#define IMU_READ_RATE 3000
/** Free-running quaternion integrate period (independent of USB/BT report cadence). */
#define IMU_QUAT_INTEGRATE_US 2000

// Access IMU config union members macro
#define CH_A_GYRO_OFFSET(axis) (imu_config->imu_a_gyro_offsets[axis])
#define CH_B_GYRO_OFFSET(axis) (imu_config->imu_b_gyro_offsets[axis])
#define CH_A_ACCEL_CFG(axis) (imu_config->imu_a_accel_config[axis])
#define CH_B_ACCEL_CFG(axis) (imu_config->imu_b_accel_config[axis])

// Macro to access the offset for a given gyro axis channel
#define IMU_GYRO_OFFSET_X(channel) (!channel ? CH_A_GYRO_OFFSET(0) : CH_B_GYRO_OFFSET(0))
#define IMU_GYRO_OFFSET_Y(channel) (!channel ? CH_A_GYRO_OFFSET(1) : CH_B_GYRO_OFFSET(1))
#define IMU_GYRO_OFFSET_Z(channel) (!channel ? CH_A_GYRO_OFFSET(2) : CH_B_GYRO_OFFSET(2))

ns_motion_quat_integrator_s _imu_quat_integrator = {0};

SNAPSHOT_TYPE(imu, imu_data_s);
snapshot_imu_t _imu_snap;

SNAPSHOT_TYPE(quat, ns_quaternion_s);
snapshot_quat_t _quat_snap;

imu_data_s _imu_data = {0};

// Gesture inputs held (bit per motion_gesture_t), written by the active core
static volatile uint8_t _imu_gesture_mask = 0;
static motion_gesture_set_s _imu_gestures = {0};

// Gravity (which way is up, mg) smoothed from the samples before gestures are added, so flicks
// can be turned to stay relative to the ground
#define IMU_UP_TAU_US 250000.0f
static float    _imu_up_mg[3] = {0, 0, 1000.0f};
static uint64_t _imu_up_last_us = 0;

static void _imu_track_up(const imu_data_s *sample, uint64_t now_us)
{
  const float dt = (_imu_up_last_us == 0 || now_us <= _imu_up_last_us) ? 0.0f : (float)(now_us - _imu_up_last_us);
  _imu_up_last_us = now_us;

  const float alpha = (dt <= 0.0f) ? 1.0f : dt / (IMU_UP_TAU_US + dt);
  const float raw[3] = {sample->ax * IMU_ACCEL_MG_PER_LSB, sample->ay * IMU_ACCEL_MG_PER_LSB, sample->az * IMU_ACCEL_MG_PER_LSB};
  for (int i = 0; i < 3; i++)
    _imu_up_mg[i] += alpha * (raw[i] - _imu_up_mg[i]);
}

static inline int16_t _imu_add_clip_int16(int16_t value, float offset)
{
  float result = (float)value + offset;
  if (result > 32767.0f)
    result = 32767.0f;
  else if (result < -32768.0f)
    result = -32768.0f;
  return (int16_t)result;
}

// A controller lying still and face up: gravity on +Z, no rotation
static void _imu_rest_sample(imu_data_s *out)
{
  out->ax = 0;
  out->ay = 0;
  out->az = IMU_ACCEL_LSB_PER_G;
  out->gx = 0;
  out->gy = 0;
  out->gz = 0;
}

// Add the gestures in progress to a finished sample (after calibration and sensitivity: the
// gesture tables already hold what hosts receive). Flicks follow the ground: they are turned to
// the gravity tracked from the samples (flat on a table while motion is off).
static void _imu_apply_gestures(imu_data_s *sample, uint64_t now_us)
{
  const uint8_t mask = _imu_gesture_mask;
  bool pressed[MOTION_GESTURE_MAX];
  for (int g = 0; g < MOTION_GESTURE_MAX; g++)
    pressed[g] = (mask >> g) & 1u;

  _imu_track_up(sample, now_us);
  const motion_gesture_out_s o = motion_gesture_update(&_imu_gestures, pressed, _imu_up_mg, now_us);

  sample->ax = _imu_add_clip_int16(sample->ax, o.accel_mg[0] / IMU_ACCEL_MG_PER_LSB);
  sample->ay = _imu_add_clip_int16(sample->ay, o.accel_mg[1] / IMU_ACCEL_MG_PER_LSB);
  sample->az = _imu_add_clip_int16(sample->az, o.accel_mg[2] / IMU_ACCEL_MG_PER_LSB);
  sample->gx = _imu_add_clip_int16(sample->gx, o.gyro_dps[0] / IMU_GYRO_DPS_PER_LSB);
  sample->gy = _imu_add_clip_int16(sample->gy, o.gyro_dps[1] / IMU_GYRO_DPS_PER_LSB);
  sample->gz = _imu_add_clip_int16(sample->gz, o.gyro_dps[2] / IMU_GYRO_DPS_PER_LSB);
}

void imu_gesture_set(const bool pressed[MOTION_GESTURE_MAX])
{
  uint8_t mask = 0;
  for (int g = 0; g < MOTION_GESTURE_MAX; g++)
    mask |= (pressed && pressed[g]) ? (uint8_t)(1u << g) : 0u;
  _imu_gesture_mask = mask;
}
ns_quaternion_s _imu_quat_state = {0};

float _imu_gyro_rad_per_lsb = 0;

int16_t _imu_average_value(int16_t first, int16_t second)
{
  int total = ((int)first + (int)second) / 2;
  if (total > 32767)
    return 32767;
  if (total < -32768)
    return -32768;
  return total;
}

static uint8_t _imu_clamp_sensitivity(uint8_t pct)
{
  if (pct < IMU_SENSITIVITY_MIN)
    return IMU_SENSITIVITY_MIN;
  if (pct > IMU_SENSITIVITY_MAX)
    return IMU_SENSITIVITY_MAX;
  return pct;
}

static inline int16_t _imu_mul_clip_int16(int16_t value, uint8_t sensitivity_pct)
{
  float result = (float)value * ((float)_imu_clamp_sensitivity(sensitivity_pct) / (float)IMU_SENSITIVITY_UNITY);
  if (result > 32767.0f)
    result = 32767.0f;
  else if (result < -32768.0f)
    result = -32768.0f;
  return (int16_t)(result + (result >= 0 ? 0.5f : -0.5f));
}

static void _imu_apply_sensitivity(imu_data_s *sample)
{
  sample->gx = _imu_mul_clip_int16(sample->gx, imu_config->imu_gyro_sensitivity[0]);
  sample->gy = _imu_mul_clip_int16(sample->gy, imu_config->imu_gyro_sensitivity[1]);
  sample->gz = _imu_mul_clip_int16(sample->gz, imu_config->imu_gyro_sensitivity[2]);

  sample->ax = _imu_mul_clip_int16(sample->ax, imu_config->imu_accel_sensitivity[0]);
  sample->ay = _imu_mul_clip_int16(sample->ay, imu_config->imu_accel_sensitivity[1]);
  sample->az = _imu_mul_clip_int16(sample->az, imu_config->imu_accel_sensitivity[2]);
}

static void _imu_default_sensitivity(void)
{
  for (int i = 0; i < 3; i++)
  {
    imu_config->imu_gyro_sensitivity[i] = IMU_GYRO_SENSITIVITY_DEFAULT;
    imu_config->imu_accel_sensitivity[i] = IMU_ACCEL_SENSITIVITY_DEFAULT;
  }
}

static void _imu_read_quaternion(uint64_t timestamp)
{
  // Built as imu_data_s like the standard path, then copied for the quaternion. Writing an
  // ns_gyrodata_s and reading it back through imu_data_s * let the compiler drop the stores
  // (strict aliasing), so every sample read zero.
  imu_data_s this_imu[3] = {0};

  uint8_t channels = imu_driver_channel_count();

  if (!imu_motion_enabled(core_current_reportformat()) || channels == 0)
  {
    // Motion off: hold still, face up
    _imu_rest_sample(&this_imu[2]);
  }
  else
  {
    // Single-channel boards read channel 0 twice (and reuse its offsets) so the
    // averaging below collapses to that single sensor.
    uint8_t ch2 = (channels >= 2) ? 1 : 0;

    imu_driver_read(0, &this_imu[0]);
    imu_driver_read(ch2, &this_imu[1]);

    this_imu[0].gx -= IMU_GYRO_OFFSET_X(0);
    this_imu[0].gy -= IMU_GYRO_OFFSET_Y(0);
    this_imu[0].gz -= IMU_GYRO_OFFSET_Z(0);

    this_imu[1].gx -= IMU_GYRO_OFFSET_X(ch2);
    this_imu[1].gy -= IMU_GYRO_OFFSET_Y(ch2);
    this_imu[1].gz -= IMU_GYRO_OFFSET_Z(ch2);

    // Average
    this_imu[2].ax = _imu_average_value(this_imu[0].ax, this_imu[1].ax);
    this_imu[2].ay = _imu_average_value(this_imu[0].ay, this_imu[1].ay);
    this_imu[2].az = _imu_average_value(this_imu[0].az, this_imu[1].az);

    this_imu[2].gx = _imu_average_value(this_imu[0].gx, this_imu[1].gx);
    this_imu[2].gy = _imu_average_value(this_imu[0].gy, this_imu[1].gy);
    this_imu[2].gz = _imu_average_value(this_imu[0].gz, this_imu[1].gz);

    _imu_apply_sensitivity(&this_imu[2]);
  }

  _imu_apply_gestures(&this_imu[2], timestamp);

  ns_gyrodata_s sample = {0};
  sample.ax = this_imu[2].ax;
  sample.ay = this_imu[2].ay;
  sample.az = this_imu[2].az;
  sample.gx = this_imu[2].gx;
  sample.gy = this_imu[2].gy;
  sample.gz = this_imu[2].gz;
  sample.timestamp_us = timestamp;

  ns_motion_update_quaternion(&_imu_quat_state, &_imu_quat_integrator, &sample, _imu_gyro_rad_per_lsb);
  snapshot_quat_write(&_quat_snap, &_imu_quat_state);
}

static void _imu_read_standard(uint64_t timestamp)
{
  imu_data_s this_imu[3] = {0};

  uint8_t channels = imu_driver_channel_count();

  if (!imu_motion_enabled(core_current_reportformat()) || channels == 0)
  {
    // Motion off: hold still, face up
    _imu_rest_sample(&this_imu[2]);
    this_imu[2].timestamp = timestamp;
  }
  else
  {
    // Single-channel boards read channel 0 twice (and reuse its offsets) so the
    // averaging below collapses to that single sensor.
    uint8_t ch2 = (channels >= 2) ? 1 : 0;

    imu_driver_read(0, &this_imu[0]);
    imu_driver_read(ch2, &this_imu[1]);

    this_imu[0].gx -= IMU_GYRO_OFFSET_X(0);
    this_imu[0].gy -= IMU_GYRO_OFFSET_Y(0);
    this_imu[0].gz -= IMU_GYRO_OFFSET_Z(0);

    this_imu[1].gx -= IMU_GYRO_OFFSET_X(ch2);
    this_imu[1].gy -= IMU_GYRO_OFFSET_Y(ch2);
    this_imu[1].gz -= IMU_GYRO_OFFSET_Z(ch2);

    // Average
    this_imu[2].ax = _imu_average_value(this_imu[0].ax, this_imu[1].ax);
    this_imu[2].ay = _imu_average_value(this_imu[0].ay, this_imu[1].ay);
    this_imu[2].az = _imu_average_value(this_imu[0].az, this_imu[1].az);

    this_imu[2].gx = _imu_average_value(this_imu[0].gx, this_imu[1].gx);
    this_imu[2].gy = _imu_average_value(this_imu[0].gy, this_imu[1].gy);
    this_imu[2].gz = _imu_average_value(this_imu[0].gz, this_imu[1].gz);
    this_imu[2].timestamp = timestamp;

    _imu_apply_sensitivity(&this_imu[2]);
  }

  _imu_apply_gestures(&this_imu[2], timestamp);

  snapshot_imu_write(&_imu_snap, &this_imu[2]);
}

// Access a pointer to the IMU data as it
void imu_access_safe(imu_data_s *out)
{
  snapshot_imu_read(&_imu_snap, out);
}

bool imu_motion_enabled(core_reportformat_t format)
{
  if (imu_driver_channel_count() == 0 || imu_config->imu_disabled == 1)
    return false;

  if (format < 0 || format >= 16)
    return true;

  return !(imu_config->imu_mode_disable_mask & (uint16_t)(1u << format));
}

// Optional access Quaternion data (If available)
void imu_quaternion_access_safe(ns_quaternion_s *out)
{
  snapshot_quat_read(&_quat_snap, out);
}

// Function we call when our IMU calibration is completed
webreport_cmd_confirm_t _calibrate_done_cb = NULL;
uint8_t _command = 0;

void _imu_calibrate_stop()
{
  rgb_clear_pulsing();

  if (_calibrate_done_cb != NULL)
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

  if (!interval_run(timestamp, IMU_READ_RATE, &calibration_interval))
    return;

  static int lx = 0;
  static int ly = 0;
  static int lz = 0;

  static int rx = 0;
  static int ry = 0;
  static int rz = 0;

  uint8_t channels = imu_driver_channel_count();

  if (_imu_calibrate_cycles_remaining > 0)
  {
    _imu_calibrate_cycles_remaining--;

    imu_data_s imu_calibration_read = {0};

    if (channels >= 1)
    {
      imu_driver_read(0, &imu_calibration_read);
      lx += imu_calibration_read.gx;
      ly += imu_calibration_read.gy;
      lz += imu_calibration_read.gz;
    }

    if (channels >= 2)
    {
      imu_driver_read(1, &imu_calibration_read);
      rx += imu_calibration_read.gx;
      ry += imu_calibration_read.gy;
      rz += imu_calibration_read.gz;
    }
  }

  if (!_imu_calibrate_cycles_remaining)
  {
    if (channels >= 1)
    {
      CH_A_GYRO_OFFSET(0) = (int8_t)(lx / IMU_CALIBRATE_CYCLES);
      CH_A_GYRO_OFFSET(1) = (int8_t)(ly / IMU_CALIBRATE_CYCLES);
      CH_A_GYRO_OFFSET(2) = (int8_t)(lz / IMU_CALIBRATE_CYCLES);
      lx = 0;
      ly = 0;
      lz = 0;
    }

    if (channels >= 2)
    {
      CH_B_GYRO_OFFSET(0) = (int8_t)(rx / IMU_CALIBRATE_CYCLES);
      CH_B_GYRO_OFFSET(1) = (int8_t)(ry / IMU_CALIBRATE_CYCLES);
      CH_B_GYRO_OFFSET(2) = (int8_t)(rz / IMU_CALIBRATE_CYCLES);
      rx = 0;
      ry = 0;
      rz = 0;
    }
    _imu_calibrate_stop();
  }
}

void _imu_calibrate_start()
{
  rgb_set_pulsing(COLOR_YELLOW);
  _imu_calibrate_cycles_remaining = IMU_CALIBRATE_CYCLES;
}

// IMU module command handler
void imu_config_cmd(imu_cmd_t cmd, webreport_cmd_confirm_t cb)
{
  switch (cmd)
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

void imu_set_read_mode(imu_mode_t mode)
{
  switch (mode)
  {
  default:
  case IMU_MODE_OFF:
    _imu_fn = NULL;
    break;

  case IMU_MODE_STANDARD:
    _imu_fn = imu_forced_task_standard;
    break;

  case IMU_MODE_QUATERNION:
    // Clear state + prev_timestamp so the first integrate after enable is not a huge Δt.
    ns_motion_quaternion_reset(&_imu_quat_state, &_imu_quat_integrator);
    _imu_fn = imu_forced_task_quaternion;
    break;
  }
}

// IMU forced task (for gated/syncronized reads)
void imu_forced_task_standard(uint64_t now_us)
{
  (void)now_us;

  // Runs without a sensor too: the samples then read still, with any gestures on top
  _imu_read_standard(sys_hal_now_us());
}

void imu_forced_task_quaternion(uint64_t now_us)
{
  static interval_s quat_interval = {0};

  // Self-pace at 2 ms even when motion ticks are report-aligned.
  if (!interval_run(now_us, IMU_QUAT_INTEGRATE_US, &quat_interval))
    return;

  _imu_read_quaternion(now_us);
}

// IMU module operational task
void imu_task(uint64_t now_us)
{
  // Jump into appropriate IMU task if it's defined
  if (_imu_calibrate_cycles_remaining)
    _imu_calibrate_function(now_us);
  else if (_imu_fn)
    _imu_fn(now_us);
}

// IMU module initialization function
bool imu_init()
{
  // Verify or default IMU
  if (imu_config->imu_config_version == CFG_BLOCK_IMU_VERSION_PREV)
  {
    // 0x12 -> 0x13 only added the per-mode mask (in old reserved space): keep calibration,
    // sensitivity and the global switch, and start with motion on in every mode
    imu_config->imu_config_version = CFG_BLOCK_IMU_VERSION;
    imu_config->imu_mode_disable_mask = 0;
    memset(imu_config->reserved, 0, sizeof(imu_config->reserved));
  }
  else if (imu_config->imu_config_version != CFG_BLOCK_IMU_VERSION)
  {
    imu_config->imu_config_version = CFG_BLOCK_IMU_VERSION;
    imu_config->imu_mode_disable_mask = 0;
    memset(imu_config->reserved, 0, sizeof(imu_config->reserved));
    imu_config->imu_disabled = 0;
    memset(&imu_config->imu_a_gyro_offsets, 0, 3);
    memset(&imu_config->imu_a_accel_config, 0, 3);
    memset(&imu_config->imu_b_gyro_offsets, 0, 3);
    memset(&imu_config->imu_b_accel_config, 0, 3);
    _imu_default_sensitivity();
  }

  // 2000 DPS
  _imu_gyro_rad_per_lsb = ns_motion_calculate_rps(2000);

  // Reset quaternion
  ns_motion_quaternion_reset(&_imu_quat_state, &_imu_quat_integrator);

  // Bring up every configured channel (weak default is a no-op when no driver).
  imu_driver_init();

  return true;
}