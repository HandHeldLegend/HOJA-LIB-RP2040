#include "battery.h"

void util_battery_monitor_task(uint32_t timestamp)
{

}

void util_battery_enable_ship_mode(void)
{
    cb_hoja_set_bluetooth_enabled(false);

    sleep_ms(300);

    const uint8_t _data[2] = {0x09, 0x51};
    int s = i2c_write_blocking(HOJA_I2C_BUS, BATTYPE_BQ25180, _data, 2, false);

    if(s == PICO_ERROR_GENERIC)
    {
        rgb_set_all(COLOR_WHITE.color);
        rgb_set_instant();
    }
    else if (s== PICO_ERROR_TIMEOUT)
    {
        rgb_set_all(COLOR_PURPLE.color);
        rgb_set_instant();
    }
    else if (s==2)
    {
        rgb_set_all(COLOR_YELLOW.color);
        rgb_set_instant();
    }
}

void util_battery_set_charge_rate(uint16_t rate_ma)
{
    // Default is 0x5
    uint8_t code = 0x5;
    uint16_t rate = rate_ma;
    if (rate_ma <= 35)
    {
        // CODE + 5 = rate
        code = rate - 5;
    }
    else
    {
        if (rate > 350)
        {
            rate = 350;
        }
        // 40 + ((CODE-31)*10) = rate
        code = ((rate - 40) / 10) + 31;
    }

    // Mask off the code to ensure charging isn't disabled
    code &= 0x7F;

    uint8_t _write[2] = {0x04, code};
    int readcheck = i2c_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _write, 2, false, 4000);
}

bool util_wire_connected()
{
    #if (HOJA_CAPABILITY_BLUETOOTH == 1)
        // We can poll the PMIC to get our plug status
        uint8_t _getstatus[1] = {0x00};
        uint8_t _readstatus[1];
        util_battery_status_s _status_converted;
        i2c_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _getstatus, 1, true, 4000);
        int readcheck = i2c_read_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _readstatus, 1, false, 4000);

        if ((readcheck == PICO_ERROR_GENERIC) || (readcheck == PICO_ERROR_TIMEOUT))
        {
            return true;
        }

        _status_converted.status = _readstatus[0];
        return _status_converted.plug_status;
    #else
        return true;
    #endif
}