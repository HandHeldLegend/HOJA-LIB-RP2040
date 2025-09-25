#include "switch/switch_motion.h"
#include "hal/sys_hal.h"

#include "hoja_bsp.h"
#if HOJA_BSP_CHIPSET==CHIPSET_RP2040
  #include "pico/float.h"
#endif

void switch_motion_pack_quat(quaternion_s *in, mode_2_s *out)
{
  static uint64_t time_ms;

  out->mode = 2;

  // Determine maximum quat component
  uint8_t max_index = 0;
  for(uint8_t i = 1; i < 4; i++)
  {
    if(fabsf(in->raw[i]) > fabsf(in->raw[max_index]))
      max_index = i;
  }

  out->max_index = max_index;

  int quaternion_30bit_components[3];

  // Exclude the max_index component from the component list, invert sign of the remaining components if it was negative. Scales the final result to a 30 bit fixed precision format where 0x40000000 is 1.0
  for (int i = 0; i < 3; ++i) {
      quaternion_30bit_components[i] = in->raw[(max_index + i + 1) & 3] * 0x40000000 * (in->raw[max_index] < 0 ? -1 : 1);
  }

  // Insert into the last sample components, do bit operations to account for split data
  out->last_sample_0  = quaternion_30bit_components[0] >> 10;

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
  time_ms = in->timestamp_ms;
  out->timestamp_start_l = time_ms & 0x1;
  out->timestamp_start_h = (time_ms  >> 1) & 0x3FF;
  out->timestamp_count = 3;

  out->accel_0.x = in->ax;
  out->accel_0.y = in->ay;
  out->accel_0.z = in->az;
}