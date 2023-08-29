#include "imu.h"

#define IMU_READ_RATE 2350 // 400Hz min
#define IMU_CALIBRATE_CYCLES 8000

imu_data_s _imu_buffer_a = {0};
imu_data_s _imu_buffer_b = {0};
bool _imu_dual_mode = false;
imu_data_s _imu_buffer_avg = {0};

uint8_t _imu_8_x[6] = {0};
uint8_t _imu_8_y[6] = {0};
uint8_t _imu_8_z[6] = {0};

uint8_t _acc_8_x[6] = {0};
uint8_t _acc_8_y[6] = {0};
uint8_t _acc_8_z[6] = {0};

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

void imu_calibrate_start()
{
  // Change leds to yellow
  rgb_s c = {
      .r = 0xFF,
      .g = 0xFF,
      .b = 0,
  };
  rgb_set_all(c.color);
  rgb_set_dirty();

  // Reset offsets to 0
  memset(global_loaded_settings.imu_0_offsets, 0x00, sizeof(int8_t) * 6);
  memset(global_loaded_settings.imu_1_offsets, 0x00, sizeof(int8_t) * 6);

  _imu_calibrate = true;
  _imu_calibrate_cycles_remaining = IMU_CALIBRATE_CYCLES;

  // Read IMU and store first reading
  cb_hoja_read_imu(&_imu_buffer_a, &_imu_buffer_b);

  //global_loaded_settings.imu_0_offsets[0] = _imu_buffer_a.ax;
  //global_loaded_settings.imu_0_offsets[1] = _imu_buffer_a.ay;
  //global_loaded_settings.imu_0_offsets[2] = _imu_buffer_a.az;
  global_loaded_settings.imu_0_offsets[3] = _imu_buffer_a.gx;
  global_loaded_settings.imu_0_offsets[4] = _imu_buffer_a.gy;
  global_loaded_settings.imu_0_offsets[5] = _imu_buffer_a.gz;

  //global_loaded_settings.imu_1_offsets[0] = _imu_buffer_b.ax;
  //global_loaded_settings.imu_1_offsets[1] = _imu_buffer_b.ay;
  //global_loaded_settings.imu_1_offsets[2] = _imu_buffer_b.az;
  global_loaded_settings.imu_1_offsets[3] = _imu_buffer_b.gx;
  global_loaded_settings.imu_1_offsets[4] = _imu_buffer_b.gy;
  global_loaded_settings.imu_1_offsets[5] = _imu_buffer_b.gz;
}

void imu_calibrate_stop()
{
  rgb_load_preset();
  rgb_set_dirty();

  settings_save_webindicate();
  settings_save();

  _imu_calibrate = false;
  
}

auto_init_mutex(imu_mutex);

void _imu_calibrate_task()
{
  // Read IMU
  cb_hoja_read_imu(&_imu_buffer_a, &_imu_buffer_b);

  //global_loaded_settings.imu_0_offsets[0] = _imu_average_value(global_loaded_settings.imu_0_offsets[0], _imu_buffer_a.ax);
  //global_loaded_settings.imu_0_offsets[1] = _imu_average_value(global_loaded_settings.imu_0_offsets[1], _imu_buffer_a.ay);
  //global_loaded_settings.imu_0_offsets[2] = _imu_average_value(global_loaded_settings.imu_0_offsets[2], _imu_buffer_a.az);
  global_loaded_settings.imu_0_offsets[3] = _imu_average_value(global_loaded_settings.imu_0_offsets[3], _imu_buffer_a.gx);
  global_loaded_settings.imu_0_offsets[4] = _imu_average_value(global_loaded_settings.imu_0_offsets[4], _imu_buffer_a.gy);
  global_loaded_settings.imu_0_offsets[5] = _imu_average_value(global_loaded_settings.imu_0_offsets[5], _imu_buffer_a.gz);

  //global_loaded_settings.imu_1_offsets[0] = _imu_average_value(global_loaded_settings.imu_1_offsets[0], _imu_buffer_b.ax);
  //global_loaded_settings.imu_1_offsets[1] = _imu_average_value(global_loaded_settings.imu_1_offsets[1], _imu_buffer_b.ay);
  //global_loaded_settings.imu_1_offsets[2] = _imu_average_value(global_loaded_settings.imu_1_offsets[2], _imu_buffer_b.az);
  global_loaded_settings.imu_1_offsets[3] = _imu_average_value(global_loaded_settings.imu_1_offsets[3], _imu_buffer_b.gx);
  global_loaded_settings.imu_1_offsets[4] = _imu_average_value(global_loaded_settings.imu_1_offsets[4], _imu_buffer_b.gy);
  global_loaded_settings.imu_1_offsets[5] = _imu_average_value(global_loaded_settings.imu_1_offsets[5], _imu_buffer_b.gz);

  _imu_calibrate_cycles_remaining--;
  if (!_imu_calibrate_cycles_remaining)
  {
    imu_calibrate_stop();
  }
}

void _imu_process_8(int16_t val, uint8_t *out)
{
  out[0] = (val & 0xFF);
  out[1] = (val & 0xFF00) >> 8;
}

// Gets the last 3 IMU datasets and puts them out
// for Nintendo Switch buffer
void imu_switch_buffer_out(uint8_t *output)
{
  uint32_t owner_out;
  while (!mutex_try_enter(&imu_mutex, &owner_out))
  {
  }

  output[0] = _acc_8_y[0];
  output[1] = _acc_8_y[1];

  output[2] = _acc_8_x[0];
  output[3] = _acc_8_x[1];

  output[4] = _acc_8_z[0];
  output[5] = _acc_8_z[1];

  output[6] = _imu_8_y[0];
  output[7] = _imu_8_y[1];

  output[8] = _imu_8_x[0];
  output[9] = _imu_8_x[1];

  output[10] = _imu_8_z[0];
  output[11] = _imu_8_z[1];

  // Group 2

  output[12] = _acc_8_y[2];
  output[13] = _acc_8_y[3];

  output[14] = _acc_8_x[2];
  output[15] = _acc_8_x[3];

  output[16] = _acc_8_z[2];
  output[17] = _acc_8_z[3];

  output[18] = _imu_8_y[2];
  output[19] = _imu_8_y[3];

  output[20] = _imu_8_x[2];
  output[21] = _imu_8_x[3];

  output[22] = _imu_8_z[2];
  output[23] = _imu_8_z[3];

  // Group 3

  output[24] = _acc_8_y[4];
  output[25] = _acc_8_y[5];

  output[26] = _acc_8_x[4];
  output[27] = _acc_8_x[5];

  output[28] = _acc_8_z[4];
  output[29] = _acc_8_z[5];

  output[30] = _imu_8_y[4];
  output[31] = _imu_8_y[5];

  output[32] = _imu_8_x[4];
  output[33] = _imu_8_x[5];

  output[34] = _imu_8_z[4];
  output[35] = _imu_8_z[5];

  _imu_read_idx = 0;

  mutex_exit(&imu_mutex);
}

void imu_set_enabled(bool enable)
{
  _imu_enabled = enable;
}

void imu_task(uint32_t timestamp)
{

  if (_imu_calibrate)
  {
    _imu_calibrate_task();
    return;
  }

  if (!_imu_enabled && (_imu_read_idx < 3))
    return;

  if (interval_run(timestamp, IMU_READ_RATE))
  {
    cb_hoja_read_imu(&_imu_buffer_a, &_imu_buffer_b);

    if (_imu_dual_mode)
    {
      _imu_buffer_avg.gx = _imu_average_value(_imu_buffer_a.gx-global_loaded_settings.imu_0_offsets[3], _imu_buffer_b.gx-global_loaded_settings.imu_1_offsets[3]);
      _imu_buffer_avg.gy = _imu_average_value(_imu_buffer_a.gy-global_loaded_settings.imu_0_offsets[4], _imu_buffer_b.gy-global_loaded_settings.imu_1_offsets[4]);
      _imu_buffer_avg.gz = _imu_average_value(_imu_buffer_a.gz-global_loaded_settings.imu_0_offsets[5], _imu_buffer_b.gz-global_loaded_settings.imu_1_offsets[5]);

      _imu_buffer_avg.ax = _imu_average_value(_imu_buffer_a.ax, _imu_buffer_b.ax);
      _imu_buffer_avg.ay = _imu_average_value(_imu_buffer_a.ay, _imu_buffer_b.ay);
      _imu_buffer_avg.az = _imu_average_value(_imu_buffer_a.az, _imu_buffer_b.az);
    }

    uint32_t owner_out;
    while (!mutex_try_enter(&imu_mutex, &owner_out))
    {
    }

    uint8_t o = _imu_read_idx * 2;

    if (_imu_dual_mode)
    {
      _imu_process_8(_imu_buffer_avg.gx, &_imu_8_x[o]);
      _imu_process_8(_imu_buffer_avg.gy, &_imu_8_y[o]);
      _imu_process_8(_imu_buffer_avg.gz, &_imu_8_z[o]);

      _imu_process_8(_imu_buffer_avg.ax, &_acc_8_x[o]);
      _imu_process_8(_imu_buffer_avg.ay, &_acc_8_y[o]);
      _imu_process_8(_imu_buffer_avg.az, &_acc_8_z[o]);
    }
    else
    {
      _imu_process_8(_imu_buffer_a.gx, &_imu_8_x[o]);
      _imu_process_8(_imu_buffer_a.gy, &_imu_8_y[o]);
      _imu_process_8(_imu_buffer_a.gz, &_imu_8_z[o]);

      _imu_process_8(_imu_buffer_a.ax, &_acc_8_x[o]);
      _imu_process_8(_imu_buffer_a.ay, &_acc_8_y[o]);
      _imu_process_8(_imu_buffer_a.az, &_acc_8_z[o]);
    }

    mutex_exit(&imu_mutex);

    if (_imu_read_idx < 3)
      _imu_read_idx++;
  }
}
