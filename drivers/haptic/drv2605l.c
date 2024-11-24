#include "drivers/haptic/drv2605l.h"

#define I2C_ERR_CHECK(func) ((func()) < 0 ? true : false)

#ifdef DRIVER_DRV2605
#if (DRIVER_DRV2605>0)

bool drv2605l_init(uint8_t i2c_instance)
{
    // Take MODE out of standby
    // uint8_t _set_mode1[] = {MODE_REGISTER, 0x00};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_mode1, 2, false);
    //
    uint8_t _set_feedback[] = {FEEDBACK_CTRL_REGISTER, FEEDBACK_CTRL_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_feedback, 2, false) < 0) return false;
    sleep_ms(5);

    uint8_t _set_control1[] = {CTRL1_REGISTER, CTRL1_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_control1, 2, false) < 0) return false;
    sleep_ms(5);

    // uint8_t _set_control2[] = {CTRL2_REGISTER, CTRL2_BYTE};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_control2, 2, false);
    // sleep_ms(10);

    uint8_t _set_control3[] = {CTRL3_REGISTER, CTRL3_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_control3, 2, false) < 0) return false;
    sleep_ms(5);

    // uint8_t _set_control4[] = {CTRL4_REGISTER, CTRL4_BYTE};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_control4, 2, false);
    // sleep_ms(10);

    // uint8_t _set_control5[] = {CTRL5_REGISTER, CTRL5_BYTE};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_control5, 2, false);
    // sleep_ms(10);

    // uint8_t _set_compensation[] = {0x18, HAPTIC_COMPENSATION};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_compensation, 2, false);

    // uint8_t _set_bemf[] = {0x19, HAPTIC_BACKEMF};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_bemf, 2, false);

    // uint8_t _set_freq[] = {OL_LRA_REGISTER, OL_LRA_PERIOD};
    // i2c_safe_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_freq, 2, false);

    uint8_t _set_odclamped[] = {ODCLAMP_REGISTER, ODCLAMP_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_odclamped, 2, false) < 0) return false;
    sleep_ms(5);

    // uint8_t _set_mode2[] = {MODE_REGISTER, STANDBY_MODE_BYTE};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_mode2, 2, false);

    uint8_t _set_mode3[] = {MODE_REGISTER, MODE_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_mode3, 2, false) < 0) return false;
    sleep_ms(5);
}

#endif
#endif