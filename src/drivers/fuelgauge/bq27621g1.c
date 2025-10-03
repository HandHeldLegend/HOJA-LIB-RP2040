#include "drivers/fuelgauge/bq27621g1.h"
#include "hal/i2c_hal.h"

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_BQ27621G1)

// Device I2C address for bq27621-G1 (7-bit form)
#define BQ27621_I2C_ADDR 0x55

// Standard command registers
#define BQ27621_CMD_CNTL     0x00
#define BQ27621_CMD_SOC      0x1C

// Control subcommands
#define BQ27621_SUBCMD_DEVICE_TYPE 0x0001
#define BQ27621_SUBCMD_RESET       0x0041

// Expected device type
#define BQ27621_DEVICE_TYPE 0x0621

bool bq27621g1_is_present(void) {
    uint8_t tx[3], rx[2];

    // Write subcommand 0x0001 to Control (reg 0x00)
    tx[0] = 0x00;   // Control register address
    tx[1] = 0x01;   // Subcommand LSB
    tx[2] = 0x00;   // Subcommand MSB

    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 3, false) < 0) {
        return false;
    }

    // Now read back 2 bytes from Control()
    uint8_t reg = 0x00;
    if (i2c_hal_write_read_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR,
                                    &reg, 1, rx, 2) < 0) {
        return false;
    }

    uint16_t devtype = (rx[1] << 8) | rx[0];
    return (devtype == BQ27621_DEVICE_TYPE);
}
#endif