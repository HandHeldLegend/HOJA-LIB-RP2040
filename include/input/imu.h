#ifndef INPUT_IMU_H
#define INPUT_IMU_H

#include "hoja_bsp.h"
#include "board_config.h"
#include <stdint.h>
#include <stdbool.h>

#include "settings_shared_types.h"
#include "input_shared_types.h"
#include "hoja_shared_types.h"

#include "ns_lib_motion.h"
#include "input/motion_gesture.h"

// HOJA's IMU scale, the same on every board (8 g, 2000 dps)
#define IMU_ACCEL_MG_PER_LSB    0.244f
#define IMU_GYRO_DPS_PER_LSB    0.07f
#define IMU_ACCEL_LSB_PER_G     4096

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
// Both channels (channel 0 twice on single-IMU boards). A driver may return samples read in the
// background since the previous call.
bool        imu_driver_read_pair(imu_data_s *a, imu_data_s *b);
const char *imu_driver_part_code(void);

void imu_access_safe(imu_data_s *out);
void imu_quaternion_access_safe(ns_quaternion_s *out);

// True when the board has an IMU and motion is on for this mode: neither turned off everywhere
// (imu_disabled) nor for this mode (imu_mode_disable_mask)
bool imu_motion_enabled(core_reportformat_t format);

// Motion gestures held right now (see input/motion_gesture.h). The active core sets them each
// time it reads input; every IMU sample then carries them, as if the controller really moved, so
// they go through exactly the same processing as real motion in every mode. While motion is off
// the samples read as a controller lying still and face up, and gestures play on top of that.
void imu_gesture_set(const bool pressed[MOTION_GESTURE_MAX]);

bool imu_init(void);

void imu_set_read_mode(imu_mode_t mode);

void imu_config_cmd(imu_cmd_t cmd, webreport_cmd_confirm_t cb);

void imu_forced_task_quaternion(uint64_t now_us);
void imu_forced_task_standard(uint64_t now_us);
void imu_task(uint64_t now_us);

#endif
