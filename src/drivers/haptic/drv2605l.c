#include "drivers/haptic/drv2605l.h"
#include "hal/i2c_hal.h"
#include "hal/sys_hal.h"

#define I2C_ERR_CHECK(func) ((func()) < 0 ? true : false)

#define RATED_VOLTAGE_HEX 0x3
#define RATED_VOLTAGE_REGISTER 0x16

#define HAPTIC_COMPENSATION 0
#define HAPTIC_BACKEMF 0

#define BLANKING_TIME_BASE (1) //(1)
#define IDISS_TIME_BASE (1)

#define N_ERM_LRA (1 << 7)
#define FB_BRAKE_FACTOR (7 << 4) // Disable brake
#define LOOP_GAIN (1U << 2)
#define HAPTIC_BEMF_GAIN 2
// Write to register 0x1A
#define FEEDBACK_CTRL_BYTE (N_ERM_LRA | FB_BRAKE_FACTOR | LOOP_GAIN | HAPTIC_BEMF_GAIN)
#define FEEDBACK_CTRL_REGISTER 0x1A

// CTRL 1 Registers START
#define STARTUP_BOOST (0 << 7) // 1 to enable
#define AC_COUPLE (1 << 5)
#define DRIVE_TIME (20) // Set default

#define CTRL1_BYTE (STARTUP_BOOST | AC_COUPLE | DRIVE_TIME)
#define CTRL1_REGISTER 0x1B
// CTRL 1 Registers END

// CTRL 2 Registers START
#define BIDIR_INPUT (1 << 7)      // Enable bidirectional input for Open-loop operation (<50% input is braking applied)
#define BRAKE_STABILIZER (0 << 6) // Improve loop stability? LOL no clue.
#define SAMPLE_TIME (3U << 4)
#define BLANKING_TIME_LOWER ((BLANKING_TIME_BASE & 0b00000011) << 2)
#define IDISS_TIME_LOWER (IDISS_TIME_BASE & 0b00000011)
// Write to register 0x1C
#define CTRL2_BYTE (BIDIR_INPUT | BRAKE_STABILIZER | SAMPLE_TIME | BLANKING_TIME_LOWER | IDISS_TIME_LOWER)
#define CTRL2_REGISTER 0x1C
// CTRL 2 Registers END

/*
    In open-loop mode, the RATED_VOLTAGE[7:0] bit is ignored. Instead, the OD_CLAMP[7:0] bit (in register 0x17)
    is used to set the rated voltage for the open-loop drive modes. For the ERM, Equation 6 calculates the rated
    voltage with a full-scale input signal. For the LRA, Equation 7 calculates the RMS voltage with a full-scale input
    signal.

    Equation 7
    V(Lra-OL_RMS) = 21.32 * (10^-3) * OD_CLAMP * sqrt(1-resonantfreq * 800 * (10^-6))
*/
#define ODCLAMP_BYTE (uint8_t)45 // Using the equation from the DRV2604L datasheet for Open Loop mode
// First value for 3.3v at 320hz is 179
// Alt value for 3v is uint8_t 163 (320hz)
// Alt value for 3v at 160hz is uint8_t 150
// Write to register 0x17
#define ODCLAMP_REGISTER 0x17

// LRA Open Loop Period (Address: 0x20)
#define OL_LRA_REGISTER 0x20
#define OL_LRA_PERIOD 0b1111111

// CTRL 3 Registers START

/*
    To configure the DRV2605L device in LRA open-loop operation, the LRA must be selected by writing the
    N_ERM_LRA bit to 1 in register 0x1A, and the LRA_OPEN_LOOP bit to 1 in register 0x1D. If PWM interface is
    used, the open-loop frequency is given by the PWM frequency divided by 128. If PWM interface is not used, the
    open-loop frequency is given by the OL_LRA_PERIOD[6:0] bit in register 0x20.
*/

#define NG_THRESH_DISABLED 0
#define NG_THRESH_2 1 // Percent
#define NG_THRESH_4 2
#define NG_THRESH_8 3
#define NG_THRESH (NG_THRESH_DISABLED << 6)

#define ERM_OPEN_LOOP (0 << 5)

#define SUPPLY_COMP_ENABLED 0
#define SUPPLY_COMP_DISABLED 1
#define SUPPLY_COMP_DIS (SUPPLY_COMP_ENABLED << 4)

#define DATA_FORMAT_RTP_SIGNED 0
#define DATA_FORMAT_RTP_UNSIGNED 1
#define DATA_FORMAT_RTP (DATA_FORMAT_RTP_SIGNED << 3)

#define LRA_DRIVE_MODE_OPC 0 // Once per cycle
#define LRA_DRIVE_MODE_TPC 1 // Twice per cycle
#define LRA_DRIVE_MODE (LRA_DRIVE_MODE_TPC << 2)

#define N_PWM_ANALOG_PWM 0
#define N_PWM_ANALOG_ANALOG 1
#define N_PWM_ANALOG (N_PWM_ANALOG_ANALOG << 1)

#define LRA_OPEN_LOOP (1)

#define CTRL3_BYTE (NG_THRESH | ERM_OPEN_LOOP | SUPPLY_COMP_DIS | DATA_FORMAT_RTP | LRA_DRIVE_MODE | N_PWM_ANALOG | LRA_OPEN_LOOP)
#define CTRL3_REGISTER 0x1D

// CTRL 3 Register END

// CTRL 4 Registers START
#define ZC_DET_TIME (0 << 6) // 100us
#define AUTO_CAL_TIME (2U << 4)
// Write to register 0x1E
#define CTRL4_BYTE (ZC_DET_TIME | AUTO_CAL_TIME)
#define CTRL4_REGISTER 0x1E
// CTRL 4 Registers END

// CTRL 5 Registers START
#define BLANKING_TIME_UPPER ((BLANKING_TIME_BASE & 0b00001100))
#define IDISS_TIME_UPPER ((IDISS_TIME_BASE & 0b00001100) >> 2)
// Write to register 0x1F
#define CTRL5_BYTE (BLANKING_TIME_UPPER | IDISS_TIME_UPPER)
#define CTRL5_REGISTER 0x1F
// CTRL 5 Registers END

// MODE Register START
#define DEV_RESET (0 << 7)
#define STANDBY (1 << 6)
#define MODE (3)
#define MODE_BYTE (DEV_RESET | MODE)
#define STANDBY_MODE_BYTE (DEV_RESET | STANDBY | 0x00)
#define MODE_REGISTER 0x01
// Write to register 0x01
#define MODE_CALIBRATE (0x07)
// MODE Register END

#define GO_REGISTER 0x0C
#define GO_SET (1U)

#define STATUS_REGISTER 0x00

#define RTP_AMPLITUDE_REGISTER 0x02
#define RTP_FREQUENCY_REGISTER 0x22
#define DRV2605_SLAVE_ADDR 0x5A

bool drv2605l_init(uint8_t i2c_instance)
{
    // Take MODE out of standby
    // uint8_t _set_mode1[] = {MODE_REGISTER, 0x00};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_mode1, 2, false);
    //
    uint8_t _set_feedback[] = {FEEDBACK_CTRL_REGISTER, FEEDBACK_CTRL_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_feedback, 2, false) < 0) return false;
    sys_hal_sleep_ms(5);

    uint8_t _set_control1[] = {CTRL1_REGISTER, CTRL1_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_control1, 2, false) < 0) return false;
    sys_hal_sleep_ms(5);

    // uint8_t _set_control2[] = {CTRL2_REGISTER, CTRL2_BYTE};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_control2, 2, false);
    // sleep_ms(10);

    uint8_t _set_control3[] = {CTRL3_REGISTER, CTRL3_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_control3, 2, false) < 0) return false;
    sys_hal_sleep_ms(5);

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
    sys_hal_sleep_ms(5);

    // uint8_t _set_mode2[] = {MODE_REGISTER, STANDBY_MODE_BYTE};
    // i2c_write_blocking(HOJA_I2C_BUS, DRV2605_SLAVE_ADDR, _set_mode2, 2, false);

    uint8_t _set_mode3[] = {MODE_REGISTER, MODE_BYTE};
    if(i2c_hal_write_blocking(i2c_instance, DRV2605_SLAVE_ADDR, _set_mode3, 2, false) < 0) return false;
    sys_hal_sleep_ms(5);
}
