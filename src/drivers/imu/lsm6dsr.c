#include "drivers/imu/lsm6dsr.h"

#include "hal/gpio_hal.h"
#include "hal/spi_hal.h"
#include "hal/i2c_hal.h"
#include "hal/sys_hal.h"
#include "board_config.h"

#include "hoja.h"

#ifdef IMU_DRIVER_LSM6DSR_SPI

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

#define SCALER_IMU 1.4f
// Function to apply inversion to data
#define APPLY_INVERSION(val, invert) (invert ? -(val) : (val))

// invert_flags uses 6 bits to invert axis. GX, GY, GZ, AX, AY, AZ is the order.
int lsm6dsr_spi_read(imu_data_s *out, uint32_t cs_gpio, uint8_t spi_instance, uint8_t invert_flags)
{
    uint8_t i[12] = {0};
    const uint8_t reg = 0x80 | IMU_OUTX_L_G;

    int ret = spi_hal_read_write_blocking(spi_instance, cs_gpio, &reg, 1, 0, &i[0], 12);

    float scaler = 0;

    bool gx_invert = (invert_flags&0b100000) != 0;
    bool gy_invert = (invert_flags&0b10000) != 0;
    bool gz_invert = (invert_flags&0b1000) != 0;

    bool ax_invert = (invert_flags&0b100) != 0;
    bool ay_invert = (invert_flags&0b10) != 0;
    bool az_invert = (invert_flags&0b1) != 0;

    scaler = (float)_imu_concat_16(i[0], i[1]);
    scaler *= SCALER_IMU;
    out->gx = APPLY_INVERSION((int16_t)scaler, gx_invert);

    scaler = (float)_imu_concat_16(i[2], i[3]);
    scaler *= SCALER_IMU;
    out->gy = APPLY_INVERSION((int16_t)scaler, gy_invert);

    scaler = (float)_imu_concat_16(i[4], i[5]);
    scaler *= SCALER_IMU;
    out->gz = APPLY_INVERSION((int16_t)scaler, gz_invert);

    out->ax = APPLY_INVERSION(_imu_concat_16(i[6], i[7]),   ax_invert);
    out->ay = APPLY_INVERSION(_imu_concat_16(i[8], i[9]),   ay_invert);
    out->az = APPLY_INVERSION(_imu_concat_16(i[10], i[11]), az_invert);

    out->retrieved = true;

    return ret;
}

int lsm6dsr_spi_init(uint32_t cs_gpio, uint8_t spi_instance)
{
    gpio_hal_init(cs_gpio, true, false);
    _imu_spi_write_register(CTRL1_XL, CTRL1_MASK, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL2_G, CTRL2_MASK, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL3_C, CTRL3_MASK, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL4_C, CTRL4_MASK | CTRL4_LPF1_SEL_G, cs_gpio, spi_instance);
    _imu_spi_write_register(CTRL6_C, CTRL6_MASK, cs_gpio, spi_instance);
    int ret = _imu_spi_write_register(CTRL8_XL, CTRL8_MASK, cs_gpio, spi_instance);

    return ret;
}

int lsm6dsr_i2c_init(uint8_t select, uint8_t i2c_instance)
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
    _imu_i2c_write_register(CTRL8_XL, CTRL8_MASK, select, i2c_instance);
    int ret = _imu_i2c_write_register(CTRL9_XL, CTRL9_MASK, select, i2c_instance);

    return ret;
}

int lsm6dsr_i2c_read(imu_data_s *out, uint8_t select, uint8_t i2c_instance, uint8_t invert_flags)
{
    const uint8_t addresses[2] = {0b1101011, 0b1101010};

    uint8_t addr = addresses[select];
    const uint8_t write_reg[1] = {IMU_OUTX_L_G};
    uint8_t i[12] = {0};

    int ret = i2c_hal_write_read_timeout_us(i2c_instance, addr, write_reg, 1, i, 12, 10000);

    float scaler = 0;

    bool gx_invert = (invert_flags&0b100000) != 0;
    bool gy_invert = (invert_flags&0b10000) != 0;
    bool gz_invert = (invert_flags&0b1000) != 0;

    bool ax_invert = (invert_flags&0b100) != 0;
    bool ay_invert = (invert_flags&0b10) != 0;
    bool az_invert = (invert_flags&0b1) != 0;

    scaler = (float)_imu_concat_16(i[0], i[1]);
    scaler *= SCALER_IMU;
    out->gx = APPLY_INVERSION((int16_t)scaler, gx_invert);

    scaler = (float)_imu_concat_16(i[2], i[3]);
    scaler *= SCALER_IMU;
    out->gy = APPLY_INVERSION((int16_t)scaler, gy_invert);

    scaler = (float)_imu_concat_16(i[4], i[5]);
    scaler *= SCALER_IMU;
    out->gz = APPLY_INVERSION((int16_t)scaler, gz_invert);

    out->ax = APPLY_INVERSION(_imu_concat_16(i[6], i[7]),   ax_invert);
    out->ay = APPLY_INVERSION(_imu_concat_16(i[8], i[9]),   ay_invert);
    out->az = APPLY_INVERSION(_imu_concat_16(i[10], i[11]), az_invert);

    out->retrieved = true;

    return ret;
}

#endif
