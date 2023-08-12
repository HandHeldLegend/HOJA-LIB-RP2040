#ifndef IMU_H
#define IMU_H

#include "hoja_includes.h"
#include "interval.h"

void imu_register(imu_data_s *data_a, imu_data_s *data_b);
void imu_set_enabled(bool enable);
void imu_switch_buffer_out(uint8_t *output);
void imu_task(uint32_t timestamp);

#endif
