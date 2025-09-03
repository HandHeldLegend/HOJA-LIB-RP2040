#ifndef INPUT_IMU_H
#define INPUT_IMU_H

#include "hoja_bsp.h"
#include "board_config.h"
#include <stdint.h>
#include <stdbool.h>

#include "settings_shared_types.h"
#include "input_shared_types.h"

#ifdef HOJA_IMU_CHAN_A_READ
    #define HOJA_IMU_DRIVER_ENABLED 1
#else
    #define HOJA_IMU_DRIVER_ENABLED 0
#endif

void imu_access_safe(imu_data_s *out);
void imu_quaternion_access_safe(quaternion_s *out);

bool imu_init();

void imu_config_cmd(imu_cmd_t cmd, webreport_cmd_confirm_t cb);
void imu_task(uint64_t timestamp);

#endif
