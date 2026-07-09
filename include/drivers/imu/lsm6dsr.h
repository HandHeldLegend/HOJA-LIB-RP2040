#ifndef DRIVERS_IMU_LSM6DSR_H
#define DRIVERS_IMU_LSM6DSR_H

#include <stdint.h>
#include <stdbool.h>

// LSM6DSR REGISTERS
#define FUNC_CFG_ACCESS 0x01
#define CTRL1_XL 0x10 // Accelerometer Activation Register - Write 0x60 to enable 416hz
#define CTRL2_G 0x11  // Gyro Activation Register - Write 0x60 to enable 416hz
#define CTRL3_C 0x12  // Used to set BDU
#define CTRL4_C 0x13
#define CTRL6_C 0x15
#define CTRL7_G 0x16
#define CTRL8_XL 0x17
#define CTRL9_XL 0x18
#define CTRL10_C 0x19
#define WHO_AM_I 0x0F

#define FUNC_MASK (0b10000000) // Enable FUNC CFG access

#define PERF_6KHZ (0b10100000)  // 6.66KHz
#define PERF_3KHZ (0b10010000)  // 3.33KHz
#define PERF_1KHZ (0b10000000)  // 1.66KHz
#define PERF_416HZ (0b01100000) // 416Hz

#define XL_LOWPASS_ON (0b10)
#define XL_LOWPASS_OFF (0b00)
#define XL_LOWPASS 0b01

// 2G
#define XL_SENS_2G (0b00000000)

// 16G
#define XL_SENS_16G (0b00001000)

// 4G
#define XL_SENS_4G (0b00001000)

// 8G
#define XL_SENS_8G (0b00001100)

// 4000dps
#define G_SENS_4000DPS (0b00000001)

// 2000dps
#define G_SENS_2000DPS (0b00001100)

// 1000dps
#define G_SENS_1000DPS (0b00001000)

// 500dps
#define G_SENS_500DPS (0b00000100)

// HPF 16 mHz
#define G_HPF_0 (0b00000000)

// HPF 65 mHz
#define G_HPF_1 (0b00010000)

// HPF 260 mHz
#define G_HPF_2 (0b00100000)

// HPF 1.0f Hz
#define G_HPF_3 (0b00110000)

#define CTRL1_MASK (XL_SENS_8G | PERF_1KHZ)// | XL_LOWPASS)
#define CTRL2_MASK (G_SENS_2000DPS | PERF_1KHZ)
#define CTRL3_MASK (0b01000100) // Address auto-increment and BDU
#define CTRL4_MASK (0b00000100) // I2C disable (Check later for LPF for gyro)
#define CTRL4_LPF1_SEL_G (0b00000010)
#define CTRL6_MASK (0b00000110) // 12.2 LPF gyro
#define CTRL7_MASK (0b0100000 | G_HPF_1)
#define CTRL8_MASK (0b11100000) // H P_SLOPE_XL_EN
#define CTRL9_MASK (0b11100010) // Disable I3C
#define CTRL10_MASK (0x38 | 0x4)

#define SPI_READ_BIT 0x80

#define IMU_OUTX_L_G 0x22
#define IMU_OUTX_L_X 0x28



// Bus a given LSM6DSR channel is wired on.
typedef enum
{
    LSM6DSR_BUS_SPI = 0,
    LSM6DSR_BUS_I2C = 1,
} lsm6dsr_bus_t;

// Per-sensor (gyro or accelerometer) settings for one channel. Currently just
// per-axis output inversion; split this way so future sensor-specific options
// (range, filtering, etc.) have an obvious home.
typedef struct
{
    bool invert_x;
    bool invert_y;
    bool invert_z;
} lsm6dsr_sensor_cfg_s;

// Per-channel configuration. Only the fields relevant to the chosen bus are
// used (SPI: spi_instance + cs_gpio; I2C: i2c_instance + select).
typedef struct
{
    lsm6dsr_bus_t bus;          // SPI or I2C
    uint8_t  spi_instance;      // SPI bus instance (bus == SPI)
    uint32_t cs_gpio;           // SPI chip-select GPIO (bus == SPI)
    uint8_t  i2c_instance;      // I2C bus instance (bus == I2C)
    uint8_t  select;            // I2C address select 0/1 (bus == I2C)
    lsm6dsr_sensor_cfg_s gyro;  // gyroscope settings
    lsm6dsr_sensor_cfg_s accel; // accelerometer settings
} lsm6dsr_channel_cfg_s;

// Driver-specific configuration for the LSM6DSR IMU.
// When HOJA_IMU_DRIVER == IMU_DRIVER_LSM6DSR, hoja_config_s embeds one of these
// as its `.imu` member; the board fills it in (main.c) and the driver reads it
// via hoja_config_get()->imu. Up to two physical sensors are averaged; set
// channel_count to 1 to use channel_a only.
typedef struct
{
    uint8_t channel_count;          // 1 or 2 physical IMUs
    lsm6dsr_channel_cfg_s channel_a;
    lsm6dsr_channel_cfg_s channel_b;
} lsm6dsr_cfg_s;

#endif