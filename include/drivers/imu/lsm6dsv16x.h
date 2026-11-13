#ifndef DRIVERS_IMU_LSM6DSV16X_H
#define DRIVERS_IMU_LSM6DSV16X_H

#include <stdbool.h>
#include <stdint.h>

// Which of the sensor's axes feeds one of HOJA's, and whether it is negated. Gyro and accelerometer
// share the chip's frame, so one mapping covers both.
typedef enum
{
    LSM6DSV16X_AXIS_X = 0,
    LSM6DSV16X_AXIS_Y = 1,
    LSM6DSV16X_AXIS_Z = 2,
} lsm6dsv16x_axis_t;

typedef struct
{
    lsm6dsv16x_axis_t source;
    bool invert;
} lsm6dsv16x_axis_cfg_s;

// Driver-specific configuration for the LSM6DSV16X IMU (I2C, one sensor).
// When HOJA_IMU_DRIVER == IMU_DRIVER_LSM6DSV16X, hoja_config_s embeds one of these as its `.imu`
// member; the board fills it in (main.c) and the driver reads it via hoja_config_get()->imu.
typedef struct
{
    uint8_t i2c_instance;
    uint8_t address;              // 0x6A or 0x6B (the SA0 pin)
    lsm6dsv16x_axis_cfg_s x;      // HOJA's X (see input/imu.h for the frame)
    lsm6dsv16x_axis_cfg_s y;
    lsm6dsv16x_axis_cfg_s z;
} lsm6dsv16x_cfg_s;

#endif
