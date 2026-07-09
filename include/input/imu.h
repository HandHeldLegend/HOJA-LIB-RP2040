#ifndef INPUT_IMU_H
#define INPUT_IMU_H

#include "hoja_bsp.h"
#include "board_config.h"
#include <stdint.h>
#include <stdbool.h>

#include "settings_shared_types.h"
#include "input_shared_types.h"

#include "ns_lib_motion.h"

typedef enum
{
    IMU_MODE_OFF = 0,
    IMU_MODE_STANDARD,
    IMU_MODE_QUATERNION,
} imu_mode_t;

// ---- IMU driver contract (weak-function model) ----
// The selected IMU driver provides strong definitions of these. Which driver
// compiles is decided by the HOJA_IMU_DRIVER gate in board_config.h. imu.c
// ships weak defaults so that when no driver is selected every call is a safe
// no-op. The driver reads its own configuration straight from the hoja config
// (hoja_config_get()->imu), whose type is shaped by the gate.
//
// imu_driver_part_code() returning NULL is the canonical "no driver present"
// signal; a real driver returns its part number (e.g. "LSM6DSR").
// imu_driver_channel_count() reports how many physical IMUs the board wired up
// (0/1/2); the device layer averages two channels or duplicates a single one.
// imu_driver_read() fills one channel's sample; init() also performs hardware
// bring-up of every configured channel.
uint8_t     imu_driver_channel_count(void);
bool        imu_driver_init(void);
bool        imu_driver_read(uint8_t channel, imu_data_s *out);
const char *imu_driver_part_code(void);

void imu_access_safe(imu_data_s *out);
void imu_quaternion_access_safe(ns_quaternion_s *out);

bool imu_init(void);

void imu_set_read_mode(imu_mode_t mode);

void imu_config_cmd(imu_cmd_t cmd, webreport_cmd_confirm_t cb);

void imu_forced_task_quaternion(uint64_t now_us);
void imu_forced_task_standard(uint64_t now_us);
void imu_task(uint64_t now_us);

#endif
