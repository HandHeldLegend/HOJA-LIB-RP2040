#include "board_config.h"

#if defined(HOJA_IMU_DRIVER) && (HOJA_IMU_DRIVER == IMU_DRIVER_LSM6DSV16X)

#include "drivers/imu/lsm6dsv16x.h"

#include "hal/i2c_hal.h"
#include "hal/sys_hal.h"
#include "hoja.h"
#include "input/imu.h"

#define LSM6DSV16X_REG_WHO_AM_I   0x0F
#define LSM6DSV16X_REG_CTRL1      0x10 // Accelerometer rate and power mode
#define LSM6DSV16X_REG_CTRL2      0x11 // Gyro rate and power mode
#define LSM6DSV16X_REG_CTRL3      0x12
#define LSM6DSV16X_REG_CTRL6      0x15 // Gyro range
#define LSM6DSV16X_REG_CTRL8      0x17 // Accelerometer range
#define LSM6DSV16X_REG_OUTX_L_G   0x22 // Gyro X, Y, Z then accelerometer X, Y, Z, low byte first

#define LSM6DSV16X_WHO_AM_I       0x70

#define LSM6DSV16X_CTRL3_SW_RESET 0x01
#define LSM6DSV16X_CTRL3_IF_INC   0x04 // The register address steps through a burst
#define LSM6DSV16X_CTRL3_BDU      0x40 // Both bytes of a value come from one sample

// High-performance mode at 960 Hz: above the fastest report rate, as the LSM6DSR boards' 1.66 kHz is
#define LSM6DSV16X_ODR_960HZ      0x09
// HOJA's scale on every board: 2000 dps (70 mdps a count) and 8 g (0.244 mg a count)
#define LSM6DSV16X_FS_G_2000DPS   0x04
#define LSM6DSV16X_FS_XL_8G       0x02

#define LSM6DSV16X_I2C_TIMEOUT_US 10000
#define LSM6DSV16X_RESET_WAIT_MS  2
#define LSM6DSV16X_RESET_TRIES    10

static bool _lsm6dsv16x_ready = false;

static inline const lsm6dsv16x_cfg_s *_lsm6dsv16x_cfg(void)
{
    const hoja_config_s *c = hoja_config_get();
    return c ? &c->imu : NULL;
}

static bool _lsm6dsv16x_write(const lsm6dsv16x_cfg_s *cfg, uint8_t reg, uint8_t value)
{
    const uint8_t write[2] = {reg, value};
    return i2c_hal_write_timeout_us(cfg->i2c_instance, cfg->address, write, 2, false,
                                    LSM6DSV16X_I2C_TIMEOUT_US) == 2;
}

static bool _lsm6dsv16x_read(const lsm6dsv16x_cfg_s *cfg, uint8_t reg, uint8_t *out, uint8_t len)
{
    return i2c_hal_read_reg_burst_us(cfg->i2c_instance, cfg->address, reg, out, len,
                                     LSM6DSV16X_I2C_TIMEOUT_US) == len;
}

static int16_t _lsm6dsv16x_axis(const int16_t raw[3], lsm6dsv16x_axis_cfg_s axis)
{
    const int16_t value = raw[(axis.source <= LSM6DSV16X_AXIS_Z) ? axis.source : LSM6DSV16X_AXIS_X];
    // -32768 has no positive twin
    if (axis.invert)
        return (value == INT16_MIN) ? INT16_MAX : (int16_t)-value;
    return value;
}

uint8_t imu_driver_channel_count(void)
{
    return (_lsm6dsv16x_cfg() != NULL) ? 1 : 0;
}

bool imu_driver_init(void)
{
    const lsm6dsv16x_cfg_s *cfg = _lsm6dsv16x_cfg();
    _lsm6dsv16x_ready = false;
    if (cfg == NULL)
        return false;

    uint8_t who = 0;
    if (!_lsm6dsv16x_read(cfg, LSM6DSV16X_REG_WHO_AM_I, &who, 1) || (who != LSM6DSV16X_WHO_AM_I))
        return false;

    // The sensor stays powered through our resets, with whatever the firmware before us set up
    // (FIFO, sensor fusion): start from its defaults. Both sensors are switched off for the reset.
    _lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL1, 0);
    _lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL2, 0);
    if (!_lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL3, LSM6DSV16X_CTRL3_SW_RESET))
        return false;
    for (uint8_t i = 0; i < LSM6DSV16X_RESET_TRIES; i++)
    {
        uint8_t ctrl3 = LSM6DSV16X_CTRL3_SW_RESET;
        sys_hal_sleep_ms(LSM6DSV16X_RESET_WAIT_MS);
        if (_lsm6dsv16x_read(cfg, LSM6DSV16X_REG_CTRL3, &ctrl3, 1) && !(ctrl3 & LSM6DSV16X_CTRL3_SW_RESET))
            break;
    }

    bool ok = _lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL3, LSM6DSV16X_CTRL3_BDU | LSM6DSV16X_CTRL3_IF_INC);
    ok = ok && _lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL6, LSM6DSV16X_FS_G_2000DPS);
    ok = ok && _lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL8, LSM6DSV16X_FS_XL_8G);
    ok = ok && _lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL1, LSM6DSV16X_ODR_960HZ);
    ok = ok && _lsm6dsv16x_write(cfg, LSM6DSV16X_REG_CTRL2, LSM6DSV16X_ODR_960HZ);

    _lsm6dsv16x_ready = ok;
    return ok;
}

bool imu_driver_read(uint8_t channel, imu_data_s *out)
{
    const lsm6dsv16x_cfg_s *cfg = _lsm6dsv16x_cfg();
    if (!_lsm6dsv16x_ready || (cfg == NULL) || (out == NULL) || (channel != 0))
        return false;

    uint8_t bytes[12];
    if (!_lsm6dsv16x_read(cfg, LSM6DSV16X_REG_OUTX_L_G, bytes, sizeof(bytes)))
        return false;

    int16_t gyro[3];
    int16_t accel[3];
    for (uint8_t i = 0; i < 3; i++)
    {
        gyro[i] = (int16_t)((bytes[(i * 2) + 1] << 8) | bytes[i * 2]);
        accel[i] = (int16_t)((bytes[(i * 2) + 7] << 8) | bytes[(i * 2) + 6]);
    }

    out->gx = _lsm6dsv16x_axis(gyro, cfg->x);
    out->gy = _lsm6dsv16x_axis(gyro, cfg->y);
    out->gz = _lsm6dsv16x_axis(gyro, cfg->z);
    out->ax = _lsm6dsv16x_axis(accel, cfg->x);
    out->ay = _lsm6dsv16x_axis(accel, cfg->y);
    out->az = _lsm6dsv16x_axis(accel, cfg->z);
    out->retrieved = true;
    return true;
}

// One sensor: both channels are the same sample
bool imu_driver_read_pair(imu_data_s *a, imu_data_s *b)
{
    if ((a == NULL) || (b == NULL) || !imu_driver_read(0, a))
        return false;

    *b = *a;
    return true;
}

const char *imu_driver_part_code(void)
{
    return "LSM6DSV16X";
}

#endif
