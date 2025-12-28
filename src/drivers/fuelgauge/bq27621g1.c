#include "drivers/fuelgauge/bq27621g1.h"
#include "hal/i2c_hal.h"

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_BQ27621G1)

// Device I2C address for bq27621-G1 (7-bit form)
#define BQ27621_I2C_ADDR 0x55

// Register addresses
#define BQ27621_FLAGS      0x06
#define BQ27621_CONTROL    0x00

// Standard command registers
#define BQ27621_CMD_CNTL     0x00
#define BQ27621_CMD_SOC      0x1C

// Control subcommands
#define BQ27621_SUBCMD_DEVICE_TYPE 0x0001
#define BQ27621_SUBCMD_RESET       0x0041
#define BQ27621_SUBCMD_SET_CFGUPDATE  0x0013
#define BQ27621_SUBCMD_SOFT_RESET     0x0042

// Data class/offsets
#define BLOCK_DATA_CONTROL  0x61
#define BLOCK_DATA_CLASS    0x3E
#define BLOCK_DATA_OFFSET   0x3F
#define BLOCK_CHECKSUM      0x60

// Subclass for State params
#define STATE_SUBCLASS      0x52

// Offsets in State subclass
#define OFFSET_DESIGN_CAP   0x4A
#define OFFSET_DESIGN_EN    0x4C
#define OFFSET_TERM_VOLT    0x50
#define OFFSET_TAPER_RATE   0x5B

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

bool bq27621g1_init(uint16_t capacity_mah)
{
    // Avoid setup for now
    return true;

    uint8_t tx[4], rx[4];
    int ret;

    // Step 1: Read Flags() to check [ITPOR]
    tx[0] = BQ27621_FLAGS;
    if (i2c_hal_write_read_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR,
                                    tx, 1, rx, 2) < 0) return false;
    uint16_t flags = (rx[1] << 8) | rx[0];
    if (!(flags & 0x0001)) {
        // [ITPOR] == 0 → already configured
        return true;
    }

    // Step 2: Enter CONFIG UPDATE (Control(0x0013))
    tx[0] = BQ27621_CONTROL;
    tx[1] = 0x13; tx[2] = 0x00;  // LSB first
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 3, false) < 0) return false;

    // Step 3: Prepare BlockData access
    tx[0] = BLOCK_DATA_CONTROL; tx[1] = 0x00;
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 2, false) < 0) return false;
    tx[0] = BLOCK_DATA_CLASS; tx[1] = STATE_SUBCLASS;
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 2, false) < 0) return false;
    tx[0] = BLOCK_DATA_OFFSET; tx[1] = 0x00;
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 2, false) < 0) return false;

    // Step 4: Program parameters
    uint16_t design_energy = (uint32_t)capacity_mah * 37 / 10; // mAh × 3.7
    uint16_t term_volt = 3200; // default 3.2 V
    uint16_t taper_rate = capacity_mah / (0.1 * 100); // assume 100 mA taper current

    // Write Design Capacity
    tx[0] = OFFSET_DESIGN_CAP;
    tx[1] = (uint8_t)(capacity_mah & 0xFF);
    tx[2] = (uint8_t)(capacity_mah >> 8);
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 3, false) < 0) return false;

    // Write Design Energy
    tx[0] = OFFSET_DESIGN_EN;
    tx[1] = (uint8_t)(design_energy & 0xFF);
    tx[2] = (uint8_t)(design_energy >> 8);
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 3, false) < 0) return false;

    // Write Terminate Voltage
    tx[0] = OFFSET_TERM_VOLT;
    tx[1] = (uint8_t)(term_volt & 0xFF);
    tx[2] = (uint8_t)(term_volt >> 8);
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 3, false) < 0) return false;

    // Write Taper Rate
    tx[0] = OFFSET_TAPER_RATE;
    tx[1] = (uint8_t)(taper_rate & 0xFF);
    tx[2] = (uint8_t)(taper_rate >> 8);
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 3, false) < 0) return false;

    // Step 5: Update checksum (simplified recompute)
    // Read old checksum
    tx[0] = BLOCK_CHECKSUM;
    if (i2c_hal_write_read_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR,
                                    tx, 1, rx, 1) < 0) return false;
    uint8_t old_chk = rx[0];
    // Quick method: just write 0xFF - (sum of new block) 
    // (For brevity: not computing whole block here, but should in production)
    uint8_t sum = (capacity_mah & 0xFF) + (capacity_mah >> 8) +
                  (design_energy & 0xFF) + (design_energy >> 8) +
                  (term_volt & 0xFF) + (term_volt >> 8) +
                  (taper_rate & 0xFF) + (taper_rate >> 8);
    uint8_t new_chk = 0xFF - sum;
    tx[0] = BLOCK_CHECKSUM;
    tx[1] = new_chk;
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 2, false) < 0) return false;

    // Step 6: Exit CONFIG UPDATE → SOFT_RESET
    tx[0] = BQ27621_CONTROL;
    tx[1] = 0x42; tx[2] = 0x00;
    if (i2c_hal_write_blocking(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, tx, 3, false) < 0) return false;

    return true;
}

uint8_t bq27621g1_get_percent(void)
{
    uint8_t reg = 0x1C;     // StateOfCharge() command code
    uint8_t rx[2] = {0};
    static uint16_t soc = 100;

    int ret = i2c_hal_write_read_timeout_us(HOJA_FUELGAUGE_I2C_INSTANCE, BQ27621_I2C_ADDR, &reg, 1, rx, 2, 1000);

    if (ret == 2) {
        soc = (rx[1] << 8) | rx[0];  // little endian
        if (soc > 100) soc = 100;    // clamp just in case
    }

    return (uint8_t)soc;
}

fuelgauge_status_s bq27621g1_get_status(void)
{
    static fuelgauge_status_s status = {0};

    status.percent = bq27621g1_get_percent();
    status.connected = true;

    return status;
}
#endif