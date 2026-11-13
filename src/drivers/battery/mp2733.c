#include "board_config.h"

#if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER == BATTERY_DRIVER_MP2733)

#include "drivers/battery/mp2733.h"

#include "devices/battery.h"
#include "hal/i2c_hal.h"
#include "hal/sys_hal.h"
#include "hoja.h"

// The charger is read, and told to disconnect the battery to power off. Its charge settings (current,
// voltage, input limit) are left as they are: the chip charges the pack by itself with the settings
// the board gives it, which is also what charges a pack too flat to run any firmware.

#define MP2733_SLAVE_ADDRESS    0x4B

#define MP2733_REG_THERMAL      0x02
#define MP2733_REG_BATFET       0x0A
#define MP2733_REG_STATUS       0x0C

// The battery switch waits about 10 s to open unless this is cleared first
#define MP2733_THERMAL_TSM_DLY  0x80
#define MP2733_BATFET_DIS       0x20

#define MP2733_STATUS_VIN_SHIFT 5
#define MP2733_STATUS_VIN_MASK  0x07 // 0: nothing plugged in
#define MP2733_STATUS_CHG_SHIFT 3
#define MP2733_STATUS_CHG_MASK  0x03

#define MP2733_CHG_NONE         0
#define MP2733_CHG_TRICKLE      1
#define MP2733_CHG_FAST         2
#define MP2733_CHG_DONE         3

#define MP2733_I2C_TIMEOUT_US   16000
// How long the system rail takes to fall once the battery is disconnected
#define MP2733_POWER_OFF_MS     250

static bool _mp2733_present = false;

static inline uint8_t _mp2733_bus(void)
{
    const hoja_config_s *c = hoja_config_get();
    return c ? c->battery.i2c_instance : 0;
}

static bool _mp2733_input_present(uint8_t status)
{
    const hoja_config_s *c = hoja_config_get();
    if (c && c->battery.input_present)
        return c->battery.input_present();
    return ((status >> MP2733_STATUS_VIN_SHIFT) & MP2733_STATUS_VIN_MASK) != 0;
}

static bool _mp2733_read(uint8_t reg, uint8_t *out)
{
    return i2c_hal_write_read_timeout_us(_mp2733_bus(), MP2733_SLAVE_ADDRESS, &reg, 1, out, 1,
                                         MP2733_I2C_TIMEOUT_US) == 1;
}

static bool _mp2733_write(uint8_t reg, uint8_t value)
{
    const uint8_t write[2] = {reg, value};
    return i2c_hal_write_timeout_us(_mp2733_bus(), MP2733_SLAVE_ADDRESS, write, 2, false,
                                    MP2733_I2C_TIMEOUT_US) == 2;
}

battery_status_s battery_driver_get_status(void)
{
    static battery_status_s status = {0};

    uint8_t raw = 0;
    if (!_mp2733_read(MP2733_REG_STATUS, &raw))
        return status;

    status.connected = true;
    status.plugged = _mp2733_input_present(raw);

    switch ((raw >> MP2733_STATUS_CHG_SHIFT) & MP2733_STATUS_CHG_MASK)
    {
        case MP2733_CHG_TRICKLE:
        case MP2733_CHG_FAST:
            status.charging = true;
            status.charging_done = false;
            break;

        case MP2733_CHG_DONE:
            status.charging = false;
            status.charging_done = true;
            break;

        default:
            status.charging = false;
            break;
    }

    if (!status.plugged)
        status.charging_done = false;

    return status;
}

// The charge current is the charger's own
bool battery_driver_set_charge_rate(uint16_t rate_ma)
{
    (void)rate_ma;
    return _mp2733_present;
}

// Power off: the charger opens the battery switch, which takes the system rail down with it. On
// external power the rail stays up, so there is nothing to do (the caller restarts).
bool battery_driver_set_ship_mode(void)
{
    uint8_t status = 0;
    uint8_t thermal = 0;
    uint8_t batfet = 0;

    if (!_mp2733_read(MP2733_REG_STATUS, &status))
        return false;
    if (_mp2733_input_present(status))
        return false;
    if (!_mp2733_read(MP2733_REG_THERMAL, &thermal) || !_mp2733_read(MP2733_REG_BATFET, &batfet))
        return false;

    if (!_mp2733_write(MP2733_REG_THERMAL, (uint8_t)(thermal & ~MP2733_THERMAL_TSM_DLY)))
        return false;
    if (!_mp2733_write(MP2733_REG_BATFET, (uint8_t)(batfet | MP2733_BATFET_DIS)))
        return false;

    // Still running after this: the switch didn't open
    sys_hal_sleep_ms(MP2733_POWER_OFF_MS);
    _mp2733_write(MP2733_REG_BATFET, batfet);
    _mp2733_write(MP2733_REG_THERMAL, thermal);
    return false;
}

bool battery_driver_init(void)
{
    uint8_t status = 0;
    _mp2733_present = _mp2733_read(MP2733_REG_STATUS, &status);
    if (!_mp2733_present)
        return false;

    // Started from a cable after a power-off: the battery is still disconnected until this is cleared
    uint8_t batfet = 0;
    if (_mp2733_read(MP2733_REG_BATFET, &batfet) && (batfet & MP2733_BATFET_DIS))
        _mp2733_write(MP2733_REG_BATFET, (uint8_t)(batfet & ~MP2733_BATFET_DIS));

    return true;
}

const char *battery_driver_part_code(void)
{
    return "MP2733";
}

// The pack is soldered in on the boards that use this charger
battery_pack_t battery_driver_pack_state(void)
{
    return _mp2733_present ? BATTERY_PACK_PRESENT : BATTERY_PACK_UNKNOWN;
}

#endif
