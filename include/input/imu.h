#ifndef IMU_H
#define IMU_H

#include "hoja_includes.h"
#include "interval.h"

void imu_pack_quat(mode_2_s *out);
void imu_get_quat(quaternion_s *out);

imu_data_s* imu_fifo_last();
imu_data_s* imu_fifo_pop();
void imu_fifo_push(imu_data_s *imu_data, uint32_t timestamp);
void imu_calibrate_start();
void imu_calibrate_stop();
void imu_register(imu_data_s *data_a, imu_data_s *data_b);
void imu_set_enabled(bool enable);
void imu_task(uint32_t timestamp);

#endif
