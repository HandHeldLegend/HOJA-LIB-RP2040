#include "board_config.h"

#if defined(HOJA_IMU_DRIVER) && (HOJA_IMU_DRIVER == IMU_DRIVER_LSM6DSR)

#include "drivers/imu/lsm6dsr.h"
#include "input/imu.h"
#include "hoja.h"

#include "hal/gpio_hal.h"
#include "hal/spi_hal.h"
#include "hal/i2c_hal.h"
#include "hal/sys_hal.h"

int _imu_spi_write_register(const uint8_t reg, const uint8_t data, uint32_t cs_gpio, uint8_t spi_instance)
{
    const uint8_t dat[2] = {reg, data};
    int ret = spi_hal_write_blocking(spi_instance, cs_gpio, dat, 2);
    sys_hal_sleep_ms(2);
    return ret;
}

int _imu_i2c_write_register(const uint8_t reg, const uint8_t data, uint8_t select, uint8_t i2c_instance)
{
    const uint8_t addresses[2] = {0b1101011, 0b1101010};
    uint8_t addr = addresses[select];
    uint8_t dat[2] = {reg, data};
    return i2c_hal_write_blocking(i2c_instance, addr, dat, 2, false);
}

int _imu_i2c_read_register(const uint8_t reg, uint8_t select, uint8_t i2c_instance, uint8_t *out)
{
    const uint8_t addresses[2] = {0b1101011, 0b1101010};
    uint8_t addr = addresses[select];
    int ret = i2c_hal_write_read_timeout_us(i2c_instance, addr, &reg, 1, out, 1, 10000); 
    return ret;
}

int16_t _imu_concat_16(uint8_t low, uint8_t high)
{
    return (int16_t)((high << 8) | low);
}

// Function to apply inversion to data
#define APPLY_INVERSION(val, invert) (invert ? -(val) : (val))

static int lsm6dsr_spi_read(imu_data_s *out, uint32_t cs_gpio, uint8_t spi_instance, lsm6dsr_sensor_cfg_s gyro, lsm6dsr_sensor_cfg_s accel)
{
    uint8_t i[12] = {0};
    const uint8_t reg = 0x80 | IMU_OUTX_L_G;

    int ret = spi_hal_read_write_blocking(spi_instance, cs_gpio, &reg, 1, 0, &i[0], 12);

    bool gx_invert = gyro.invert_x;
    bool gy_invert = gyro.invert_y;
    bool gz_invert = gyro.invert_z;

    bool ax_invert = accel.invert_x;
    bool ay_invert = accel.invert_y;
    bool az_invert = accel.invert_z;

    // Sensitivity scaling is applied in imu.c from imu_config multipliers.
    out->gx = APPLY_INVERSION(_imu_concat_16(i[0], i[1]), gx_invert);
    out->gy = APPLY_INVERSION(_imu_concat_16(i[2], i[3]), gy_invert);
    out->gz = APPLY_INVERSION(_imu_concat_16(i[4], i[5]), gz_invert);

    out->ax = APPLY_INVERSION(_imu_concat_16(i[6], i[7]),   ax_invert);
    out->ay = APPLY_INVERSION(_imu_concat_16(i[8], i[9]),   ay_invert);
    out->az = APPLY_INVERSION(_imu_concat_16(i[10], i[11]), az_invert);

    out->retrieved = true;

    return ret;
}

static int lsm6dsr_spi_init(uint32_t cs_gpio, uint8_t spi_instance)
{
    gpio_hal_init(cs_gpio, true, false);
    _imu_spi_write_register(CTRL1_XL, CTRL1_MASK, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL2_G, CTRL2_MASK, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL3_C, CTRL3_MASK, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL4_C, CTRL4_MASK | CTRL4_LPF1_SEL_G, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL6_C, CTRL6_MASK, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL7_G, CTRL7_MASK, cs_gpio, spi_instance);
    int ret = _imu_spi_write_register(CTRL8_XL, CTRL8_MASK, cs_gpio, spi_instance);

    return ret;
}

static int lsm6dsr_i2c_init(uint8_t select, uint8_t i2c_instance)
{
    uint8_t who = 0;
    _imu_i2c_read_register(WHO_AM_I, select, i2c_instance, &who);

    /* Verified reading is working
    if(who == 0x6B)
    {
        hoja_set_notification_status(COLOR_RED);
    }*/
    
    _imu_i2c_write_register(CTRL1_XL, CTRL1_MASK, select, i2c_instance);
    _imu_i2c_write_register(CTRL2_G, CTRL2_MASK, select, i2c_instance);
    _imu_i2c_write_register(CTRL3_C, CTRL3_MASK, select, i2c_instance);
    _imu_i2c_write_register(CTRL4_C, CTRL4_LPF1_SEL_G, select, i2c_instance);
    _imu_i2c_write_register(CTRL6_C, CTRL6_MASK, select, i2c_instance);
    _imu_i2c_write_register(CTRL7_G, CTRL7_MASK, select, i2c_instance);
    _imu_i2c_write_register(CTRL8_XL, CTRL8_MASK, select, i2c_instance);
    int ret = _imu_i2c_write_register(CTRL9_XL, CTRL9_MASK, select, i2c_instance);

    return ret;
}

static int lsm6dsr_i2c_read(imu_data_s *out, uint8_t select, uint8_t i2c_instance, lsm6dsr_sensor_cfg_s gyro, lsm6dsr_sensor_cfg_s accel)
{
    const uint8_t addresses[2] = {0b1101011, 0b1101010};

    uint8_t addr = addresses[select];
    const uint8_t write_reg[1] = {IMU_OUTX_L_G};
    uint8_t i[12] = {0};

    int ret = i2c_hal_write_read_timeout_us(i2c_instance, addr, write_reg, 1, i, 12, 10000);

    bool gx_invert = gyro.invert_x;
    bool gy_invert = gyro.invert_y;
    bool gz_invert = gyro.invert_z;

    bool ax_invert = accel.invert_x;
    bool ay_invert = accel.invert_y;
    bool az_invert = accel.invert_z;

    // Sensitivity scaling is applied in imu.c from imu_config multipliers.
    out->gx = APPLY_INVERSION(_imu_concat_16(i[0], i[1]), gx_invert);
    out->gy = APPLY_INVERSION(_imu_concat_16(i[2], i[3]), gy_invert);
    out->gz = APPLY_INVERSION(_imu_concat_16(i[4], i[5]), gz_invert);

    out->ax = APPLY_INVERSION(_imu_concat_16(i[6], i[7]),   ax_invert);
    out->ay = APPLY_INVERSION(_imu_concat_16(i[8], i[9]),   ay_invert);
    out->az = APPLY_INVERSION(_imu_concat_16(i[10], i[11]), az_invert);

    out->retrieved = true;

    return ret;
}

// ---- Strong driver contract overrides (weak-function model) ----

static inline const lsm6dsr_cfg_s *_cfg(void)
{
    const hoja_config_s *c = hoja_config_get();
    return c ? &c->imu : NULL;
}

static const lsm6dsr_channel_cfg_s *_channel(const lsm6dsr_cfg_s *cfg, uint8_t idx)
{
    return (idx == 0) ? &cfg->channel_a : &cfg->channel_b;
}

static int _channel_init(const lsm6dsr_channel_cfg_s *ch)
{
    if (ch->bus == LSM6DSR_BUS_I2C)
        return lsm6dsr_i2c_init(ch->select, ch->i2c_instance);
    return lsm6dsr_spi_init(ch->cs_gpio, ch->spi_instance);
}

static int _channel_read(const lsm6dsr_channel_cfg_s *ch, imu_data_s *out)
{
    if (ch->bus == LSM6DSR_BUS_I2C)
        return lsm6dsr_i2c_read(out, ch->select, ch->i2c_instance, ch->gyro, ch->accel);
    return lsm6dsr_spi_read(out, ch->cs_gpio, ch->spi_instance, ch->gyro, ch->accel);
}

uint8_t imu_driver_channel_count(void)
{
    const lsm6dsr_cfg_s *cfg = _cfg();
    if (cfg == NULL)
        return 0;
    return (cfg->channel_count > 2) ? 2 : cfg->channel_count;
}

bool imu_driver_init(void)
{
    const lsm6dsr_cfg_s *cfg = _cfg();
    if (cfg == NULL || cfg->channel_count == 0)
        return false;

    uint8_t count = imu_driver_channel_count();
    for (uint8_t i = 0; i < count; i++)
        _channel_init(_channel(cfg, i));

    return true;
}

bool imu_driver_read(uint8_t channel, imu_data_s *out)
{
    const lsm6dsr_cfg_s *cfg = _cfg();
    if (cfg == NULL || out == NULL || channel >= imu_driver_channel_count())
        return false;

    return _channel_read(_channel(cfg, channel), out) >= 0;
}

const char *imu_driver_part_code(void)
{
    return "LSM6DSR";
}

#endif
