#include "imu.h"

#define IMU_READ_RATE 2000 // 500KHz 
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

// Add data to our FIFO
void imu_fifo_push(imu_data_s *imu_data)
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

  rgb_flash(c.color);

  // Reset offsets to 0
  memset(global_loaded_settings.imu_0_offsets, 0x00, sizeof(int8_t) * 6);
  memset(global_loaded_settings.imu_1_offsets, 0x00, sizeof(int8_t) * 6);

  _imu_calibrate = true;
  _imu_calibrate_cycles_remaining = IMU_CALIBRATE_CYCLES;
  _imu_calibrate_init = false;

  // Read IMU and store first reading
  cb_hoja_read_imu(&_imu_buffer_a, &_imu_buffer_b);

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

  rgb_init(global_loaded_settings.rgb_mode, -1);

  settings_save_webindicate();
  settings_save();

  _imu_calibrate = false;
  
}

void _imu_calibrate_task()
{
  // Read IMU
  cb_hoja_read_imu(&_imu_buffer_a, &_imu_buffer_b);

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
}

// Depreciated
//void _imu_process_8(int16_t val, uint8_t *out)
//{
//  out[0] = (val & 0xFF);
//  out[1] = (val & 0xFF00) >> 8;
//}

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

  if (!_imu_enabled)
    return;

  if (interval_run(timestamp, IMU_READ_RATE))
  {
    cb_hoja_read_imu(&_imu_buffer_a, &_imu_buffer_b);

    // If we received two buffers, average A and B
    if (_imu_buffer_b.retrieved && _imu_buffer_a.retrieved)
    {
      _imu_buffer_avg.gx = _imu_average_value(_imu_buffer_a.gx-global_loaded_settings.imu_0_offsets[3], _imu_buffer_b.gx-global_loaded_settings.imu_1_offsets[3]);
      _imu_buffer_avg.gy = _imu_average_value(_imu_buffer_a.gy-global_loaded_settings.imu_0_offsets[4], _imu_buffer_b.gy-global_loaded_settings.imu_1_offsets[4]);
      _imu_buffer_avg.gz = _imu_average_value(_imu_buffer_a.gz-global_loaded_settings.imu_0_offsets[5], _imu_buffer_b.gz-global_loaded_settings.imu_1_offsets[5]);

      _imu_buffer_avg.ax = _imu_average_value(_imu_buffer_a.ax-global_loaded_settings.imu_0_offsets[0], _imu_buffer_b.ax-global_loaded_settings.imu_1_offsets[0]);
      _imu_buffer_avg.ay = _imu_average_value(_imu_buffer_a.ay-global_loaded_settings.imu_0_offsets[1] , _imu_buffer_b.ay-global_loaded_settings.imu_1_offsets[1]);
      _imu_buffer_avg.az = _imu_average_value(_imu_buffer_a.az-global_loaded_settings.imu_0_offsets[2], _imu_buffer_b.az-global_loaded_settings.imu_1_offsets[2]);

      imu_fifo_push(&_imu_buffer_avg);
    }
    else if (_imu_buffer_a.retrieved)
    {
      imu_fifo_push(&_imu_buffer_a);
    }

    _imu_buffer_a.retrieved = false;
    _imu_buffer_b.retrieved = false;
  }

  
}
