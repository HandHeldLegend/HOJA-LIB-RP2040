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
__attribute__((weak)) bool imu_driver_read_pair(imu_data_s *a, imu_data_s *b)
{
  const uint8_t ch2 = (imu_driver_channel_count() >= 2) ? 1 : 0;
  bool ok = imu_driver_read(0, a);
  ok &= imu_driver_read(ch2, b);
  return ok;
}

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

// Gravity (which way is up) smoothed from the samples before gestures are added, so flicks can be
// turned to stay relative to the ground. Integer math: this runs on every sample, at up to 1 kHz.
#define IMU_UP_TAU_US 250000u
#define IMU_UP_WEIGHT_SHIFT 12 // Filter weight in 1/4096ths
static int32_t  _imu_up_q8[3] = {0, 0, IMU_ACCEL_LSB_PER_G * 256}; // Accel LSB x 256
static uint64_t _imu_up_last_us = 0;

static void _imu_track_up(const imu_data_s *sample, uint64_t now_us)
{
  const int32_t raw_q8[3] = {sample->ax * 256, sample->ay * 256, sample->az * 256};

  // First sample: start from it
  if (_imu_up_last_us == 0 || now_us <= _imu_up_last_us)
  {
    _imu_up_last_us = now_us;
    for (int i = 0; i < 3; i++)
      _imu_up_q8[i] = raw_q8[i];
    return;
  }

  uint32_t dt = (uint32_t)(now_us - _imu_up_last_us);
  if (dt > IMU_UP_TAU_US)
    dt = IMU_UP_TAU_US;
  _imu_up_last_us = now_us;

  // Low-pass, each sample weighted dt / (tau + dt)
  const int32_t weight = (int32_t)((dt << IMU_UP_WEIGHT_SHIFT) / (IMU_UP_TAU_US + dt));
  for (int i = 0; i < 3; i++)
    _imu_up_q8[i] += (int32_t)(((int64_t)(raw_q8[i] - _imu_up_q8[i]) * weight) >> IMU_UP_WEIGHT_SHIFT);
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

  _imu_track_up(sample, now_us);

  // Nothing held or playing: leave the sample alone (the float work below is costly on the M0+)
  if (!mask && !motion_gesture_busy(&_imu_gestures))
    return;

  bool pressed[MOTION_GESTURE_MAX];
  for (int g = 0; g < MOTION_GESTURE_MAX; g++)
    pressed[g] = (mask >> g) & 1u;

  const float up_mg[3] = {
    (float)_imu_up_q8[0] * (IMU_ACCEL_MG_PER_LSB / 256.0f),
    (float)_imu_up_q8[1] * (IMU_ACCEL_MG_PER_LSB / 256.0f),
    (float)_imu_up_q8[2] * (IMU_ACCEL_MG_PER_LSB / 256.0f),
  };
  const motion_gesture_out_s o = motion_gesture_update(&_imu_gestures, pressed, up_mg, now_us);

  sample->ax = _imu_add_clip_int16(sample->ax, o.accel_mg[0] * (1.0f / IMU_ACCEL_MG_PER_LSB));
  sample->ay = _imu_add_clip_int16(sample->ay, o.accel_mg[1] * (1.0f / IMU_ACCEL_MG_PER_LSB));
  sample->az = _imu_add_clip_int16(sample->az, o.accel_mg[2] * (1.0f / IMU_ACCEL_MG_PER_LSB));
  sample->gx = _imu_add_clip_int16(sample->gx, o.gyro_dps[0] * (1.0f / IMU_GYRO_DPS_PER_LSB));
  sample->gy = _imu_add_clip_int16(sample->gy, o.gyro_dps[1] * (1.0f / IMU_GYRO_DPS_PER_LSB));
  sample->gz = _imu_add_clip_int16(sample->gz, o.gyro_dps[2] * (1.0f / IMU_GYRO_DPS_PER_LSB));
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

// Integer math, rounded half away from zero: six of these run for every motion sample, and in
// software floating point they took 27 us of each read
static inline int16_t _imu_mul_clip_int16(int16_t value, uint8_t sensitivity_pct)
{
  int32_t scaled = (int32_t)value * (int32_t)_imu_clamp_sensitivity(sensitivity_pct);
  int32_t result = (scaled >= 0) ? ((scaled + (IMU_SENSITIVITY_UNITY / 2)) / IMU_SENSITIVITY_UNITY)
                                 : -((-scaled + (IMU_SENSITIVITY_UNITY / 2)) / IMU_SENSITIVITY_UNITY);
  if (result > 32767)
    return 32767;
  if (result < -32768)
    return -32768;
  return (int16_t)result;
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

static void _imu_apply_accel_sensitivity(imu_data_s *sample)
{
  sample->ax = _imu_mul_clip_int16(sample->ax, imu_config->imu_accel_sensitivity[0]);
  sample->ay = _imu_mul_clip_int16(sample->ay, imu_config->imu_accel_sensitivity[1]);
  sample->az = _imu_mul_clip_int16(sample->az, imu_config->imu_accel_sensitivity[2]);
}

// ---- The gyro's bias, kept up while the gamepad rests ----
//
// A gyro reads a little off zero when nothing turns, by an amount that moves with temperature and
// time, and a host takes it as a slow turn. The calibration the user runs measures it once, to a
// whole count a channel. Here it is measured again whenever the gamepad is resting, to a fraction
// of a count, and taken off every sample.
//
// Resting is told from the channels themselves, half a second at a look. What one channel reads
// less what the other reads has no turning in it, only the two sensors' own noise: so however
// noisy this pair is, at whatever temperature, that difference says how much the channels would
// vary with nothing moving. A look is still when the channels vary no more than a few times that.
// A board with one channel has no such measure and is held to a fixed figure instead. Three still
// looks in a row that agree to a count set the bias; each still look after them moves it a
// quarter of the way to what it measured.
#define IMU_BIAS_LOOK_US 500000u
#define IMU_BIAS_LOOK_MIN 32u  // Samples a look needs
#define IMU_BIAS_LOOKS 3u      // Still looks in a row before the bias is taken from them
// A sample this many counts from the bias is the gamepad moving, whatever the rest of the look says
#define IMU_BIAS_REACH 96
// Still: the two channels' variances together within this many times the difference's ...
#define IMU_BIAS_STILL_TIMES 6u
// ... with this much allowed where the difference shows none at all (both sat on one value), in
// quarters of a count squared
#define IMU_BIAS_STILL_FLOOR_Q2 2u
// One channel alone: its variance under this, in quarters of a count squared
#define IMU_BIAS_SINGLE_LIMIT_Q2 4u
// Looks agree: their means within this of each other (256ths of a count)
#define IMU_BIAS_AGREE_Q8 256

#define IMU_GYRO_FRAC_BITS 12

typedef struct
{
  bool started;
  bool dual;
  int32_t bias[2][3];   // A channel's reading at rest, in 256ths of a count
  // The look in hand
  uint64_t look_from_us;
  uint16_t n;
  bool moving;
  int16_t base[2][3];   // The bias to the nearest count as the look began: sums are about this
  int32_t sum[2][3];
  uint32_t sq[2][3];
  uint32_t sq_diff[3];
  // The still looks so far
  uint8_t streak;
  int32_t streak_sum[2][3];
  int32_t streak_min[2][3];
  int32_t streak_max[2][3];
  int32_t owed[3];      // What rounding has kept back from the reports so far
} imu_bias_s;

static imu_bias_s _imu_bias = {0};


// ---- The bias, learned while the gamepad is moved ----
//
// A rest is the only time the bias can be read straight off, and a gamepad in use is not put
// down. But the accelerometers say which way is down whatever the hands do, and a gyro with a
// bias left on it turns "down" a little wrong, all the time. So "down" is kept here as the gyro
// turns it and pulled gently to where the accelerometers have it; and the pull that takes, steady
// in one direction, is the bias showing. It goes on to the bias both channels share.
//
// It cannot show the bias about "down" itself: turning that way moves nothing the accelerometers
// can see. That part is left to the rests.
//
// Run over 45 s of a GCU 2 in the hands, with a bias of 0.68 degrees a second put on: what could
// be shown of it was down from 0.5 to 0.02 after 35 s held and turned slowly. Left learning
// through quick moves and shakes it went back out to 0.4, the accelerometers then being full of
// the hands' own pushing; so it learns only once the gamepad has been turning gently, and
// weighing 1 g, for a second.
#define IMU_LEVEL_ONE_G 4096           // Accelerometer counts a g, at 8 g full scale
#define IMU_LEVEL_TRUST 614            // "Down" is taken from a sample within this of 1 g (0.15 g)
#define IMU_LEVEL_CALM_G 246           // Calm: within this of 1 g (0.06 g) ...
#define IMU_LEVEL_CALM_SUM_Q8 219429   // ... and every axis under 30 degrees a second (the channels' sum, 256ths)
#define IMU_LEVEL_CALM_US 1000000u     // ... for this long
#define IMU_LEVEL_GAP_US 50000u        // No sample for this long: "down" is taken afresh
#define IMU_LEVEL_LIMIT_Q24 (48 << 24) // The most it will put on, either way: 48 counts, 3.4 degrees a second
// Radians a 256th of a count of the channels' sum turns in a microsecond, times 2^30, as n / 2^23
#define IMU_LEVEL_TURN_NUM 21493
// The pull is 1 / 2^20 of the difference a microsecond: "down" follows the accelerometers in about a second
#define IMU_LEVEL_PULL_SHIFT 20
// Counts of bias (2^24ths) for a pull held a microsecond: a tenth of the pull's rate, so the bias
// follows in about ten seconds
#define IMU_LEVEL_LEARN_NUM 1373

typedef struct
{
  bool started;
  bool learning;
  int32_t down[3];    // Which way is down in the sensors' axes, a unit vector in 2^30ths
  int32_t common[3];  // The bias learned here, on both channels alike, in 2^24ths of a count
  uint64_t last_us;
  uint32_t calm_us;
} imu_level_s;

static imu_level_s _imu_level = {0};

static uint32_t _imu_isqrt(uint32_t value)
{
  uint32_t root = 0;
  uint32_t bit = 1u << 30;
  while (bit > value)
    bit >>= 2;
  while (bit)
  {
    if (value >= (root + bit))
    {
      value -= root + bit;
      root = (root >> 1) + bit;
    }
    else
      root >>= 1;
    bit >>= 2;
  }
  return root;
}

static inline int32_t _imu_mul_q30(int32_t a, int32_t b)
{
  return (int32_t)(((int64_t)a * b) >> 30);
}

// sum_q8: the channels' sum with every bias off, in 256ths of a count. acc: the channels' mean.
static void _imu_level_update(const int32_t sum_q8[3], const imu_data_s *acc, uint64_t now_us)
{
  const int32_t a[3] = {acc->ax, acc->ay, acc->az};
  const uint32_t mag = _imu_isqrt((uint32_t)((a[0] * a[0]) + (a[1] * a[1]) + (a[2] * a[2])));
  const int32_t off_g = (int32_t)mag - IMU_LEVEL_ONE_G;
  const bool trusted = (mag > 0u) && (off_g < IMU_LEVEL_TRUST) && (off_g > -IMU_LEVEL_TRUST);

  int32_t seen[3] = {0, 0, 0};
  if (trusted)
  {
    const int32_t scale = (int32_t)((1u << 30) / mag);
    for (int axis = 0; axis < 3; axis++)
      seen[axis] = a[axis] * scale;
  }

  const uint64_t gap_us = now_us - _imu_level.last_us;
  if (!_imu_level.started || (gap_us > IMU_LEVEL_GAP_US))
  {
    if (!trusted)
      return;
    memcpy(_imu_level.down, seen, sizeof(seen));
    _imu_level.started = true;
    _imu_level.last_us = now_us;
    _imu_level.calm_us = 0;
    _imu_level.learning = false;
    return;
  }
  _imu_level.last_us = now_us;
  const int32_t dt_us = (int32_t)gap_us;

  // The gyro turns "down" (the sensors turn one way, so what they see turns the other)
  int32_t turn[3];
  bool gentle = true;
  for (int axis = 0; axis < 3; axis++)
  {
    turn[axis] = (int32_t)(((int64_t)sum_q8[axis] * dt_us * IMU_LEVEL_TURN_NUM) >> 23);
    if ((sum_q8[axis] > IMU_LEVEL_CALM_SUM_Q8) || (sum_q8[axis] < -IMU_LEVEL_CALM_SUM_Q8))
      gentle = false;
  }
  int32_t *down = _imu_level.down;
  const int32_t turned[3] = {
      down[0] + _imu_mul_q30(turn[1], down[2]) - _imu_mul_q30(turn[2], down[1]),
      down[1] + _imu_mul_q30(turn[2], down[0]) - _imu_mul_q30(turn[0], down[2]),
      down[2] + _imu_mul_q30(turn[0], down[1]) - _imu_mul_q30(turn[1], down[0]),
  };
  memcpy(down, turned, sizeof(turned));

  const bool calm = trusted && gentle && (off_g < IMU_LEVEL_CALM_G) && (off_g > -IMU_LEVEL_CALM_G);
  if (!calm)
    _imu_level.calm_us = 0;
  else if (_imu_level.calm_us < 0x7FFFFFFFu)
    _imu_level.calm_us += (uint32_t)dt_us;
  _imu_level.learning = _imu_level.calm_us >= IMU_LEVEL_CALM_US;

  if (trusted)
  {
    int32_t pull[3];
    for (int axis = 0; axis < 3; axis++)
      pull[axis] = seen[axis] - down[axis];

    if (_imu_level.learning)
    {
      // The pull across "down" is the bias left on, a turn at a time
      const int32_t across[3] = {
          _imu_mul_q30(down[1], pull[2]) - _imu_mul_q30(down[2], pull[1]),
          _imu_mul_q30(down[2], pull[0]) - _imu_mul_q30(down[0], pull[2]),
          _imu_mul_q30(down[0], pull[1]) - _imu_mul_q30(down[1], pull[0]),
      };
      for (int axis = 0; axis < 3; axis++)
      {
        int32_t common = _imu_level.common[axis] - (int32_t)(((int64_t)across[axis] * dt_us * IMU_LEVEL_LEARN_NUM) >> 30);
        if (common > IMU_LEVEL_LIMIT_Q24)
          common = IMU_LEVEL_LIMIT_Q24;
        if (common < -IMU_LEVEL_LIMIT_Q24)
          common = -IMU_LEVEL_LIMIT_Q24;
        _imu_level.common[axis] = common;
      }
    }

    for (int axis = 0; axis < 3; axis++)
      down[axis] += (int32_t)(((int64_t)pull[axis] * dt_us) >> IMU_LEVEL_PULL_SHIFT);
  }

  // Kept a unit long: one step of Newton's each sample is all the little it strays needs
  const int32_t len2 = _imu_mul_q30(down[0], down[0]) + _imu_mul_q30(down[1], down[1]) + _imu_mul_q30(down[2], down[2]);
  const int32_t fix = (3 << 29) - (len2 >> 1);
  for (int axis = 0; axis < 3; axis++)
    down[axis] = _imu_mul_q30(down[axis], fix);
}

static void _imu_bias_look_begin(uint64_t now_us)
{
  _imu_bias.look_from_us = now_us;
  _imu_bias.n = 0;
  _imu_bias.moving = false;
  for (int ch = 0; ch < 2; ch++)
  {
    for (int axis = 0; axis < 3; axis++)
    {
      const int32_t bias = _imu_bias.bias[ch][axis];
      _imu_bias.base[ch][axis] = (int16_t)((bias >= 0) ? ((bias + 128) / 256) : -((-bias + 128) / 256));
      _imu_bias.sum[ch][axis] = 0;
      _imu_bias.sq[ch][axis] = 0;
    }
  }
  for (int axis = 0; axis < 3; axis++)
    _imu_bias.sq_diff[axis] = 0;
}

// From the calibration the user ran: where tracking starts, and starts again after another
static void _imu_bias_reset(uint64_t now_us)
{
  memset(&_imu_bias, 0, sizeof(_imu_bias));
  _imu_bias.dual = (imu_driver_channel_count() >= 2);
  const uint8_t ch2 = _imu_bias.dual ? 1 : 0;
  _imu_bias.bias[0][0] = IMU_GYRO_OFFSET_X(0) * 256;
  _imu_bias.bias[0][1] = IMU_GYRO_OFFSET_Y(0) * 256;
  _imu_bias.bias[0][2] = IMU_GYRO_OFFSET_Z(0) * 256;
  _imu_bias.bias[1][0] = IMU_GYRO_OFFSET_X(ch2) * 256;
  _imu_bias.bias[1][1] = IMU_GYRO_OFFSET_Y(ch2) * 256;
  _imu_bias.bias[1][2] = IMU_GYRO_OFFSET_Z(ch2) * 256;
  _imu_bias.started = true;
  _imu_bias_look_begin(now_us);
}

// A look is over: was the gamepad still through it, and what does that say of the bias
static void _imu_bias_look_end(void)
{
  const uint32_t n = _imu_bias.n;
  int32_t mean[2][3];
  bool still = !_imu_bias.moving;

  for (int axis = 0; axis < 3; axis++)
  {
    uint64_t var[2];
    for (int ch = 0; ch < 2; ch++)
    {
      const int32_t sum = _imu_bias.sum[ch][axis];
      mean[ch][axis] = ((int32_t)_imu_bias.base[ch][axis] * 256) + ((sum * 256) / (int32_t)n);
      // n squared times the variance
      var[ch] = ((uint64_t)n * _imu_bias.sq[ch][axis]) - ((uint64_t)((int64_t)sum * sum));
    }

    const uint64_t n2 = (uint64_t)n * n;
    if (_imu_bias.dual)
    {
      const int32_t sum_diff = _imu_bias.sum[0][axis] - _imu_bias.sum[1][axis];
      const uint64_t var_diff = ((uint64_t)n * _imu_bias.sq_diff[axis]) - ((uint64_t)((int64_t)sum_diff * sum_diff));
      if ((var[0] + var[1]) > ((IMU_BIAS_STILL_TIMES * var_diff) + ((IMU_BIAS_STILL_FLOOR_Q2 * n2) / 4u)))
        still = false;
    }
    else if (var[0] > ((IMU_BIAS_SINGLE_LIMIT_Q2 * n2) / 4u))
      still = false;
  }

  if (!still)
  {
    _imu_bias.streak = 0;
    return;
  }

  bool agree = true;
  for (int ch = 0; ch < 2; ch++)
  {
    for (int axis = 0; axis < 3; axis++)
    {
      const int32_t m = mean[ch][axis];
      if (!_imu_bias.streak)
      {
        _imu_bias.streak_sum[ch][axis] = 0;
        _imu_bias.streak_min[ch][axis] = m;
        _imu_bias.streak_max[ch][axis] = m;
      }
      _imu_bias.streak_sum[ch][axis] += m;
      if (m < _imu_bias.streak_min[ch][axis])
        _imu_bias.streak_min[ch][axis] = m;
      if (m > _imu_bias.streak_max[ch][axis])
        _imu_bias.streak_max[ch][axis] = m;
      if ((_imu_bias.streak_max[ch][axis] - _imu_bias.streak_min[ch][axis]) > IMU_BIAS_AGREE_Q8)
        agree = false;
    }
  }

  if (!agree)
  {
    // A slow change under the looks: not a rest to measure from
    _imu_bias.streak = 0;
    return;
  }

  if (_imu_bias.streak < 255u)
    _imu_bias.streak++;
  if (_imu_bias.streak < IMU_BIAS_LOOKS)
    return;

  for (int ch = 0; ch < 2; ch++)
  {
    for (int axis = 0; axis < 3; axis++)
    {
      if (_imu_bias.streak == IMU_BIAS_LOOKS)
        _imu_bias.bias[ch][axis] = _imu_bias.streak_sum[ch][axis] / (int32_t)IMU_BIAS_LOOKS;
      else
        _imu_bias.bias[ch][axis] += (mean[ch][axis] - _imu_bias.bias[ch][axis]) / 4;
    }
  }
  for (int axis = 0; axis < 3; axis++)
    _imu_level.common[axis] = 0;
}

// a and b as read, offsets still on. out's gyro axes are written: the two channels' mean with
// their biases off, scaled by the sensitivity, and only then rounded to the count a report holds,
// with what the rounding left over owed to the next sample.
static void _imu_gyro_combine(const imu_data_s *a, const imu_data_s *b, imu_data_s *out, uint64_t now_us)
{
  if (!_imu_bias.started)
    _imu_bias_reset(now_us);

  const int16_t raw[2][3] = {{a->gx, a->gy, a->gz}, {b->gx, b->gy, b->gz}};

  // The look
  if (!_imu_bias.moving)
  {
    int32_t dev[2][3];
    for (int ch = 0; ch < 2; ch++)
    {
      for (int axis = 0; axis < 3; axis++)
      {
        dev[ch][axis] = (int32_t)raw[ch][axis] - _imu_bias.base[ch][axis];
        if ((dev[ch][axis] > IMU_BIAS_REACH) || (dev[ch][axis] < -IMU_BIAS_REACH))
          _imu_bias.moving = true;
      }
    }
    if (!_imu_bias.moving)
    {
      for (int axis = 0; axis < 3; axis++)
      {
        const int32_t diff = dev[0][axis] - dev[1][axis];
        _imu_bias.sum[0][axis] += dev[0][axis];
        _imu_bias.sum[1][axis] += dev[1][axis];
        _imu_bias.sq[0][axis] += (uint32_t)(dev[0][axis] * dev[0][axis]);
        _imu_bias.sq[1][axis] += (uint32_t)(dev[1][axis] * dev[1][axis]);
        _imu_bias.sq_diff[axis] += (uint32_t)(diff * diff);
      }
    }
  }
  if (_imu_bias.n < 0xFFFFu)
    _imu_bias.n++;
  if ((now_us - _imu_bias.look_from_us) >= IMU_BIAS_LOOK_US)
  {
    if (_imu_bias.moving)
    {
      _imu_bias.streak = 0;
    }
    else if (_imu_bias.n >= IMU_BIAS_LOOK_MIN)
      _imu_bias_look_end();
    _imu_bias_look_begin(now_us);
  }

  // The channels' sum with their biases off, and what was learned in the hands off both
  int32_t sums_q8[3];
  for (int axis = 0; axis < 3; axis++)
    sums_q8[axis] = (((int32_t)raw[0][axis] + raw[1][axis]) * 256) - _imu_bias.bias[0][axis] - _imu_bias.bias[1][axis] -
                    (_imu_level.common[axis] / (1 << 15));

  // out's accelerometer axes are the channels' mean by now
  _imu_level_update(sums_q8, out, now_us);

  // The report
  const int32_t one = 2 << IMU_GYRO_FRAC_BITS; // A report's count is the two channels' mean
  int16_t counts[3];
  for (int axis = 0; axis < 3; axis++)
  {
    const int32_t sum_q8 = sums_q8[axis];
    const int64_t level = (int64_t)sum_q8 * (1 << (IMU_GYRO_FRAC_BITS - 8));
    const int64_t scaled = (level * _imu_clamp_sensitivity(imu_config->imu_gyro_sensitivity[axis])) / IMU_SENSITIVITY_UNITY;
    const int64_t total = scaled + _imu_bias.owed[axis];
    int64_t count = (total >= 0) ? ((total + (one / 2)) / one) : -((-total + (one / 2)) / one);
    _imu_bias.owed[axis] = (int32_t)(total - (count * one));

    // Past what a report holds nothing is owed
    if (count > 32767)
    {
      count = 32767;
      _imu_bias.owed[axis] = 0;
    }
    if (count < -32768)
    {
      count = -32768;
      _imu_bias.owed[axis] = 0;
    }
    counts[axis] = (int16_t)count;
  }

  out->gx = counts[0];
  out->gy = counts[1];
  out->gz = counts[2];
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

    imu_driver_read_pair(&this_imu[0], &this_imu[1]);

    (void)ch2;

    // Average
    this_imu[2].ax = _imu_average_value(this_imu[0].ax, this_imu[1].ax);
    this_imu[2].ay = _imu_average_value(this_imu[0].ay, this_imu[1].ay);
    this_imu[2].az = _imu_average_value(this_imu[0].az, this_imu[1].az);

    // The channels' biases come off here, kept up from the gamepad's rests
    _imu_gyro_combine(&this_imu[0], &this_imu[1], &this_imu[2], timestamp);

    _imu_apply_accel_sensitivity(&this_imu[2]);
  }

  _imu_apply_gestures(&this_imu[2], timestamp);

  // The sample as the standard mode would publish it, for the config app's motion view while a
  // game uses the quaternion (through the dongle or BLE)
  this_imu[2].timestamp = timestamp;
  snapshot_imu_write(&_imu_snap, &this_imu[2]);

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

    imu_driver_read_pair(&this_imu[0], &this_imu[1]);

    (void)ch2;

    // Average
    this_imu[2].ax = _imu_average_value(this_imu[0].ax, this_imu[1].ax);
    this_imu[2].ay = _imu_average_value(this_imu[0].ay, this_imu[1].ay);
    this_imu[2].az = _imu_average_value(this_imu[0].az, this_imu[1].az);

    // The channels' biases come off here, kept up from the gamepad's rests
    _imu_gyro_combine(&this_imu[0], &this_imu[1], &this_imu[2], timestamp);
    this_imu[2].timestamp = timestamp;

    _imu_apply_accel_sensitivity(&this_imu[2]);
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

  // Tracking starts again from what was just measured
  _imu_bias.started = false;

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