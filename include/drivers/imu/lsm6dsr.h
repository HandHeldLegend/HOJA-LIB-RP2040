#ifndef DRIVERS_IMU_LSM6DSR_H
#define DRIVERS_IMU_LSM6DSR_H

#include "input/imu.h"

#include <stdint.h>
#include <stdbool.h>

// LSM6DSR REGISTERS
#define FUNC_CFG_ACCESS 0x01
#define CTRL1_XL 0x10 // Accelerometer Activation Register - Write 0x60 to enable 416hz
#define CTRL2_G 0x11  // Gyro Activation Register - Write 0x60 to enable 416hz
#define CTRL3_C 0x12  // Used to set BDU
#define CTRL4_C 0x13
#define CTRL6_C 0x15
#define CTRL8_XL 0x17
#define CTRL9_XL 0x18
#define CTRL10_C 0x19
#define WHO_AM_I 0x0F

#define FUNC_MASK (0b10000000) // Enable FUNC CFG access

#define PERF_6KHZ (0b10100000)  // 6.66KHz
#define PERF_1KHZ (0b10000000)  // 1.66KHz
#define PERF_416HZ (0b01100000) // 416Hz

#define XL_LOWPASS (0b00000001)

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

#define CTRL1_MASK (XL_SENS_8G | PERF_1KHZ)
#define CTRL2_MASK (G_SENS_2000DPS | PERF_1KHZ)
#define CTRL3_MASK (0b00000100) // Address auto-increment
#define CTRL4_MASK (0b00000100) // I2C disable (Check later for LPF for gyro)
#define CTRL4_LPF1_SEL_G (0b00000010)
#define CTRL6_MASK (0b00000110) // 12.2 LPF gyro
#define CTRL8_MASK (0b11100000) // H P_SLOPE_XL_EN
#define CTRL9_MASK (0b11100010) // Disable I3C
#define CTRL10_MASK (0x38 | 0x4)

#define SPI_READ_BIT 0x80

#define IMU_OUTX_L_G 0x22
#define IMU_OUTX_L_X 0x28



// Driver channel configs
// Maximum 2 channels

#if defined(HOJA_IMU_CHAN_A_DRIVER) && (HOJA_IMU_CHAN_A_DRIVER==IMU_DRIVER_LSM6DSR_SPI)

    // Requires SPI to function
    #if (HOJA_BSP_HAS_SPI==0)
        #error "LSM6DSR driver requires SPI." 
    #endif

    #ifndef HOJA_IMU_CHAN_A_CS_PIN
        #error "HOJA_IMU_CHAN_A_CS_PIN undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_A_SPI_INSTANCE
        #error "HOJA_IMU_CHAN_A_SPI_INSTANCE undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_A_INVERT_FLAGS
        #error "HOJA_IMU_CHAN_A_INVERT_FLAGS undefined in board_config.h. 6 bits, one per axis. Gyro XYZ, Acc XYZ" 
    #endif

    #ifdef HOJA_IMU_CHAN_A_READ
        #error "HOJA_IMU_CHAN_A_READ define conflict."
    #endif

    #define HOJA_IMU_CHAN_A_READ(out)   lsm6dsr_spi_read(out, HOJA_IMU_CHAN_A_CS_PIN, HOJA_IMU_CHAN_A_SPI_INSTANCE, HOJA_IMU_CHAN_A_INVERT_FLAGS)
    #define HOJA_IMU_CHAN_A_INIT()      lsm6dsr_spi_init(HOJA_IMU_CHAN_A_CS_PIN, HOJA_IMU_CHAN_A_SPI_INSTANCE);
#endif

#if defined(HOJA_IMU_CHAN_B_DRIVER) && (HOJA_IMU_CHAN_B_DRIVER==IMU_DRIVER_LSM6DSR_SPI)
    #ifndef HOJA_IMU_CHAN_B_CS_PIN
        #error "HOJA_IMU_CHAN_B_CS_PIN undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_B_SPI_INSTANCE
        #error "HOJA_IMU_CHAN_B_SPI_INSTANCE undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_B_INVERT_FLAGS
        #error "HOJA_IMU_CHAN_B_INVERT_FLAGS undefined in board_config.h. 6 bits, one per axis. Gyro XYZ, Acc XYZ" 
    #endif

    #ifdef HOJA_IMU_CHAN_B_READ
        #error "HOJA_IMU_CHAN_B_READ define conflict."
    #endif

    #define HOJA_IMU_CHAN_B_READ(out)   lsm6dsr_spi_read(out, HOJA_IMU_CHAN_B_CS_PIN, HOJA_IMU_CHAN_B_SPI_INSTANCE, HOJA_IMU_CHAN_B_INVERT_FLAGS)
    #define HOJA_IMU_CHAN_B_INIT()      lsm6dsr_spi_init(HOJA_IMU_CHAN_B_CS_PIN, HOJA_IMU_CHAN_B_SPI_INSTANCE);
#endif

#if defined(HOJA_IMU_CHAN_A_DRIVER) && (HOJA_IMU_CHAN_A_DRIVER==IMU_DRIVER_LSM6DSR_I2C)
    #ifndef HOJA_IMU_CHAN_A_I2C_INSTANCE
        #error "HOJA_IMU_CHAN_A_I2C_INSTANCE undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_A_SELECT
        #error "HOJA_IMU_CHAN_A_SELECT undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_A_INVERT_FLAGS
        #error "HOJA_IMU_CHAN_A_INVERT_FLAGS undefined in board_config.h. 6 bits, one per axis. Gyro XYZ, Acc XYZ" 
    #endif

    #ifdef HOJA_IMU_CHAN_A_READ
        #error "HOJA_IMU_CHAN_A_READ define conflict."
    #endif

    #define HOJA_IMU_CHAN_A_READ(out)   lsm6dsr_i2c_read(out, HOJA_IMU_CHAN_A_SELECT, HOJA_IMU_CHAN_A_I2C_INSTANCE, HOJA_IMU_CHAN_A_INVERT_FLAGS)
    #define HOJA_IMU_CHAN_A_INIT()      lsm6dsr_i2c_init(HOJA_IMU_CHAN_A_SELECT, HOJA_IMU_CHAN_A_I2C_INSTANCE)
#endif

#if defined(HOJA_IMU_CHAN_B_DRIVER) && (HOJA_IMU_CHAN_B_DRIVER==IMU_DRIVER_LSM6DSR_I2C)
    #ifndef HOJA_IMU_CHAN_B_I2C_INSTANCE
        #error "HOJA_IMU_CHAN_B_I2C_INSTANCE undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_B_SELECT
        #error "HOJA_IMU_CHAN_B_SELECT undefined in board_config.h" 
    #endif

    #ifndef HOJA_IMU_CHAN_B_INVERT_FLAGS
        #error "HOJA_IMU_CHAN_B_INVERT_FLAGS undefined in board_config.h. 6 bits, one per axis. Gyro XYZ, Acc XYZ" 
    #endif

    #ifdef HOJA_IMU_CHAN_B_READ
        #error "HOJA_IMU_CHAN_B_READ define conflict."
    #endif

    #define HOJA_IMU_CHAN_B_READ(out)   lsm6dsr_i2c_read(out, HOJA_IMU_CHAN_B_SELECT, HOJA_IMU_CHAN_B_I2C_INSTANCE, HOJA_IMU_CHAN_B_INVERT_FLAGS)
    #define HOJA_IMU_CHAN_B_INIT()      lsm6dsr_i2c_init(HOJA_IMU_CHAN_B_SELECT, HOJA_IMU_CHAN_B_I2C_INSTANCE)
#endif

int lsm6dsr_spi_init(uint32_t cs_gpio, uint8_t spi_instance);
int lsm6dsr_spi_read(imu_data_s *out, uint32_t cs_gpio, uint8_t spi_instance, uint8_t invert_flags);
int lsm6dsr_i2c_init(uint8_t select, uint8_t i2c_instance);
int lsm6dsr_i2c_read(imu_data_s *out, uint8_t select, uint8_t i2c_instance, uint8_t invert_flags);

#endif