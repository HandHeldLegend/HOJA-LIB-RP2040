#include "drivers/fuelgauge/bq27621g1.h"
#include "hal/i2c_hal.h"
#include "hal/sys_hal.h"

// This driver mirrors the verified ChromiumOS EC bq27621-G1 battery driver
// (driver/battery/bq27621_g1.c), adapted to the HOJA I2C HAL. Sequence,
// register addresses, checksum method, and timing quirks are taken from that
// driver rather than re-derived from the datasheet.

#define BQ27621_ADDR                  0x55
#define BQ27621_TYPE_ID               0x0621

// Standard / extended command registers
#define REG_CTRL                      0x00
#define REG_FLAGS                     0x06
#define REG_STATE_OF_CHARGE           0x1c
#define REG_DESIGN_CAPACITY           0x3c
#define REG_DATA_CLASS                0x3e
#define REG_DATA_BLOCK                0x3f
#define REG_BLOCK_DATA_CHECKSUM       0x60
#define REG_BLOCK_DATA_CONTROL        0x61

// State block (data subclass 82): parameter addresses within the BlockData
// command window (0x40 + offset).
#define STATE_BLOCK_OFFSET            82
#define STATE_BLOCK_DESIGN_CAPACITY   0x43
#define STATE_BLOCK_DESIGN_ENERGY     0x45
#define STATE_BLOCK_TERMINATE_VOLTAGE 0x49
#define STATE_BLOCK_TAPER_RATE        0x54

// Control() subcommands
#define CONTROL_CONTROL_STATUS        0x0000
#define CONTROL_DEVICE_TYPE           0x0001
#define CONTROL_SET_CFGUPDATE         0x0013
#define CONTROL_RESET                 0x0041
#define CONTROL_SOFT_RESET            0x0042
#define CONTROL_UNSEAL                0x8000

// CONTROL_STATUS bits
#define STATUS_SS                     0x2000

// Flags() bits
#define FLAGS_ITPOR                   0x0020
#define FLAGS_CFGUPD                  0x0010

// Fallback tuning values, used when the board leaves a cfg field as 0.
#define BQ27621_DEFAULT_TERMINATE_MV 3200   // System minimum operating voltage (mV)
#define BQ27621_DEFAULT_TAPER_MA     100    // Charger taper/termination current (mA)

// Active driver config, set at the top of each public vtable entry so the
// low-level helpers can reach the I2C instance and tuning values.
static const bq27621g1_cfg_s *_cfg = NULL;

static inline uint16_t _term_mv(void)
{
    return (_cfg && _cfg->terminate_mv) ? _cfg->terminate_mv : BQ27621_DEFAULT_TERMINATE_MV;
}

static inline uint16_t _taper_ma(void)
{
    return (_cfg && _cfg->taper_ma) ? _cfg->taper_ma : BQ27621_DEFAULT_TAPER_MA;
}

// The ChromiumOS driver requires the bus at <= 100 kbps ("Delays need to be
// added for correct operation at > 100Kbps"). The HOJA bus runs at 400 kHz,
// so every gauge transaction temporarily overrides the baud to 100 kHz via the
// HAL's odbaud calls. All transfers use a finite timeout so a missing gauge
// can never hang the boot path.
#define BQ27621_I2C_BAUD_KHZ   100
#define BQ27621_I2C_TIMEOUT_US 20000

// ChromiumOS i2c_write16/read16 semantics: 16-bit data is LSB-first on the
// wire. Data-memory parameters are big-endian in the gauge, so they get
// endian-swapped before writing (matching the ChromiumOS ENDIAN_SWAP_2B).
#define ENDIAN_SWAP_2B(x) ((uint16_t)((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8)))

static int _bq_w8(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg, val };
    return i2c_hal_write_timeout_us_odbaud(_cfg->i2c_instance, BQ27621_ADDR,
                                           tx, 2, false, BQ27621_I2C_TIMEOUT_US,
                                           BQ27621_I2C_BAUD_KHZ);
}

static int _bq_w16(uint8_t reg, uint16_t val)
{
    uint8_t tx[3] = { reg, (uint8_t)(val & 0xff), (uint8_t)(val >> 8) };
    return i2c_hal_write_timeout_us_odbaud(_cfg->i2c_instance, BQ27621_ADDR,
                                           tx, 3, false, BQ27621_I2C_TIMEOUT_US,
                                           BQ27621_I2C_BAUD_KHZ);
}

static int _bq_read(uint8_t reg, uint8_t *dst, size_t len)
{
    int ret = i2c_hal_write_timeout_us_odbaud(_cfg->i2c_instance, BQ27621_ADDR,
                                              &reg, 1, true, BQ27621_I2C_TIMEOUT_US,
                                              BQ27621_I2C_BAUD_KHZ);
    if (ret < 0) return ret;
    return i2c_hal_read_timeout_us_odbaud(_cfg->i2c_instance, BQ27621_ADDR,
                                          dst, len, false, BQ27621_I2C_TIMEOUT_US,
                                          BQ27621_I2C_BAUD_KHZ);
}

static int _bq_r8(uint8_t reg, uint8_t *val)
{
    return _bq_read(reg, val, 1);
}

static int _bq_r16(uint8_t reg, uint16_t *val)
{
    uint8_t rx[2];
    int ret = _bq_read(reg, rx, 2);
    if (ret >= 0) *val = (uint16_t)(rx[0] | (rx[1] << 8));
    return ret;
}

// ---- Parameter computation (per the ChromiumOS board-parameter scheme) ----

// Batteries below 150 mAh are scaled x10, above 6 Ah scaled /10. Voltages and
// Taper Rate use unscaled values, exactly as the ChromiumOS driver does.
static uint16_t _bq_scaled_capacity(uint16_t capacity_mah)
{
    if (capacity_mah < 150)  return (uint16_t)(capacity_mah * 10u);
    if (capacity_mah > 6000) return (uint16_t)(capacity_mah / 10u);
    return capacity_mah;
}

static void _bq_compute_params(uint16_t capacity_mah, uint16_t *design_cap,
                               uint16_t *design_energy, uint16_t *term_volt,
                               uint16_t *taper_rate)
{
    *design_cap    = _bq_scaled_capacity(capacity_mah);
    // Design Energy = Design Capacity x 3.7 (ChromiumOS board parameter docs)
    *design_energy = (uint16_t)(((uint32_t)(*design_cap) * 37u) / 10u);
    *term_volt     = _term_mv();
    // Taper Rate = Design Capacity / (0.1 * Taper Current), unscaled capacity
    *taper_rate    = (uint16_t)(((uint32_t)capacity_mah * 10u) / (uint32_t)_taper_ma());
}

// ---- Sequencing helpers (1:1 with the ChromiumOS driver) ----

static bool _bq_probe(void)
{
    uint16_t id = 0;
    if (_bq_w16(REG_CTRL, CONTROL_DEVICE_TYPE) < 0) return false;
    if (_bq_r16(REG_CTRL, &id) < 0) return false;
    return id == BQ27621_TYPE_ID;
}

static bool _bq_unseal_if_needed(void)
{
    uint16_t status = 0;
    if (_bq_w16(REG_CTRL, CONTROL_CONTROL_STATUS) < 0) return false;
    if (_bq_r16(REG_CTRL, &status) < 0) return false;
    if (!(status & STATUS_SS)) return true; // Already unsealed

    if (_bq_w16(REG_CTRL, CONTROL_UNSEAL) < 0) return false;
    if (_bq_w16(REG_CTRL, CONTROL_UNSEAL) < 0) return false;
    return true;
}

// ChromiumOS re-issues SET_CFGUPDATE on every poll iteration; entering the
// mode can take up to a second.
static bool _bq_enter_config_update(void)
{
    for (int tries = 0; tries < 200; tries++)
    {
        uint16_t flags = 0;
        if (_bq_w16(REG_CTRL, CONTROL_SET_CFGUPDATE) < 0) return false;
        if (_bq_r16(REG_FLAGS, &flags) < 0) return false;
        if (flags & FLAGS_CFGUPD) return true;
        sys_hal_sleep_ms(5);
    }
    return false;
}

static bool _bq_enter_block_mode(uint8_t data_class)
{
    if (_bq_w8(REG_BLOCK_DATA_CONTROL, 0) < 0) return false;
    if (_bq_w8(REG_DATA_CLASS, data_class) < 0) return false;
    if (_bq_w8(REG_DATA_BLOCK, 0) < 0) return false;
    // ChromiumOS: "Shouldn't be needed, doesn't work without it."
    sys_hal_sleep_us(500);
    return true;
}

#define CHECKSUM_2B(x) (((x) & 0xff) + (((x) >> 8) & 0xff))

// One full configuration pass: unseal, enter CONFIG UPDATE, patch the four
// State-block parameters using the replacement-checksum method, soft-reset.
static bool _bq_config_params(uint16_t capacity_mah)
{
    uint16_t design_cap, design_energy, term_volt, taper_rate;
    _bq_compute_params(capacity_mah, &design_cap, &design_energy, &term_volt, &taper_rate);

    if (!_bq_unseal_if_needed())   return false;
    if (!_bq_enter_config_update()) return false;
    if (!_bq_enter_block_mode(STATE_BLOCK_OFFSET)) return false;

    uint8_t old_csum = 0;
    if (_bq_r8(REG_BLOCK_DATA_CHECKSUM, &old_csum) < 0) return false;

    // Replacement checksum: remove the old parameter bytes from the running
    // sum, add the new ones, and complement at the end (per ChromiumOS).
    int checksum = 0xff - old_csum;

    const uint8_t  addrs[4]  = { STATE_BLOCK_DESIGN_CAPACITY, STATE_BLOCK_DESIGN_ENERGY,
                                 STATE_BLOCK_TERMINATE_VOLTAGE, STATE_BLOCK_TAPER_RATE };
    const uint16_t values[4] = { design_cap, design_energy, term_volt, taper_rate };

    for (int i = 0; i < 4; i++)
    {
        uint16_t old_param = 0;
        if (_bq_r16(addrs[i], &old_param) < 0) return false;
        checksum -= CHECKSUM_2B(old_param);
    }

    for (int i = 0; i < 4; i++)
    {
        // Data memory is big-endian; swap so the MSB goes out first.
        uint16_t wire = ENDIAN_SWAP_2B(values[i]);
        if (_bq_w16(addrs[i], wire) < 0) return false;
        checksum += CHECKSUM_2B(wire);
    }

    checksum = 0xff - (0xff & checksum);
    if (_bq_w8(REG_BLOCK_DATA_CHECKSUM, (uint8_t)checksum) < 0) return false;

    // Exit CONFIG UPDATE; also clears [ITPOR].
    if (_bq_w16(REG_CTRL, CONTROL_SOFT_RESET) < 0) return false;

    // Intentionally not SEALED afterward so the configuration can be read
    // back for verification.
    return true;
}

// ---- Internal init/verify ----

static bool bq27621g1_verify(uint16_t capacity_mah);

static bool bq27621g1_init_internal(uint16_t capacity_mah)
{
    if (capacity_mah == 0) return false;

    // Skip reprogramming only when the gauge is already configured ([ITPOR]
    // clear) AND every stored parameter matches the target values. Checking
    // all parameters (not just capacity) means changes to terminate voltage
    // or taper settings in the board config also trigger reprogramming.
    uint16_t flags = 0;
    if (_bq_r16(REG_FLAGS, &flags) < 0) return false;

    if (!(flags & FLAGS_ITPOR) && bq27621g1_verify(capacity_mah))
        return true;

    if (_bq_config_params(capacity_mah)) return true;

    // ChromiumOS retries once more after a full reset.
    if (_bq_w16(REG_CTRL, CONTROL_RESET) < 0) return false;
    sys_hal_sleep_ms(10);
    return _bq_config_params(capacity_mah);
}

// Reads the four configured parameters back out of the State block and
// confirms they match what bq27621g1_init_internal() programs for this capacity.
static bool bq27621g1_verify(uint16_t capacity_mah)
{
    if (capacity_mah == 0) return false;

    uint16_t design_cap, design_energy, term_volt, taper_rate;
    _bq_compute_params(capacity_mah, &design_cap, &design_energy, &term_volt, &taper_rate);

    if (!_bq_enter_block_mode(STATE_BLOCK_OFFSET)) return false;

    const uint8_t  addrs[4]    = { STATE_BLOCK_DESIGN_CAPACITY, STATE_BLOCK_DESIGN_ENERGY,
                                   STATE_BLOCK_TERMINATE_VOLTAGE, STATE_BLOCK_TAPER_RATE };
    const uint16_t expected[4] = { design_cap, design_energy, term_volt, taper_rate };

    for (int i = 0; i < 4; i++)
    {
        uint16_t readback = 0;
        if (_bq_r16(addrs[i], &readback) < 0) return false;
        // Reads compose LSB-first but the stored value is big-endian, so the
        // expected value must be swapped the same way it was for writing.
        if (readback != ENDIAN_SWAP_2B(expected[i])) return false;
    }

    return true;
}

static uint8_t bq27621g1_get_percent(void)
{
    static uint8_t soc = 100;

    uint8_t val = 0;
    if (_bq_r8(REG_STATE_OF_CHARGE, &val) >= 0)
    {
        soc = (val > 100) ? 100 : val;
    }

    return soc;
}

// ---- Driver vtable entry points ----

static bool bq27621g1_drv_init(const fuelgauge_driver_s *drv, uint16_t capacity_mah)
{
    _cfg = (const bq27621g1_cfg_s *)drv->cfg;

    // Presence detection (folded into init): confirm the device type ID.
    if (!_bq_probe()) return false;

    return bq27621g1_init_internal(capacity_mah);
}

static fuelgauge_status_s bq27621g1_drv_get_status(const fuelgauge_driver_s *drv)
{
    _cfg = (const bq27621g1_cfg_s *)drv->cfg;

    fuelgauge_status_s status = {0};
    status.percent = bq27621g1_get_percent();
    status.connected = true;

    return status;
}

const fuelgauge_driver_api_s bq27621g1_fuelgauge_api = {
    .part_code  = "BQ27621G1",
    .init       = bq27621g1_drv_init,
    .get_status = bq27621g1_drv_get_status,
};
