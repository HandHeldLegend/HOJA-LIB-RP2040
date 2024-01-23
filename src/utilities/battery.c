#include "battery.h"
#include "interval.h"

// Battery monitor task we run when we are in a wired mode
void util_battery_monitor_task_usb(uint32_t timestamp)
{
    #if (HOJA_CAPABILITY_BATTERY == 1)

    // Check status every 64ms
    if(interval_run(timestamp, 64000))
    {
        static bool _connected = true;
        _connected = util_wire_connected();
        if(!_connected)
        {
            hoja_shutdown();
        }
    }

    #else
    (void) timestamp;
    #endif
}

bool util_battery_comms_check()
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
        // We can poll the PMIC to get our plug status
        uint8_t _getstatus[1] = {0x00};
        uint8_t _readstatus[1] = {0x00};
        util_battery_status_s _status_converted;
        i2c_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _getstatus, 1, true, 10000);
        int readcheck = i2c_read_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _readstatus, 1, false, 10000);
        if(readcheck < 1) return false;
        return true;
    #else
        return false;
    #endif
}

// Battery monitor task we run when wireless
void util_battery_monitor_task_wireless(uint32_t timestamp)
{
    #if (HOJA_CAPABILITY_BATTERY == 1)

    // Check status every 64ms
    if(interval_run(timestamp, 64000))
    {
        
    }

    #else
    (void) timestamp;
    #endif
}

void util_battery_enable_ship_mode(void)
{
    cb_hoja_set_bluetooth_enabled(false);

    #if (HOJA_CAPABILITY_BATTERY == 1)
    int s2 = 0;
    while(!s2)
    {
        const uint8_t _data[2] = {0x09, 0x41};
        s2 = i2c_write_blocking(HOJA_I2C_BUS, BATTYPE_BQ25180, _data, 2, false);

        if(s2 == PICO_ERROR_GENERIC)
        {
            hoja_shutdown_instant();
        }
        else if (s2== PICO_ERROR_TIMEOUT)
        {
            hoja_shutdown_instant();
        }
        else if (s2==2)
        {
            //rgb_indicate(COLOR_YELLOW.color);
        }

        sleep_ms(1000);

        hoja_shutdown_instant();
    }
    #endif

    hoja_shutdown_instant();

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
    
    #if (HOJA_CAPABILITY_BATTERY == 1)
    int readcheck = i2c_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _write, 2, false, 4000);
    #endif
}

bool util_wire_connected()
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
        // We can poll the PMIC to get our plug status
        uint8_t _getstatus[1] = {0x00};
        uint8_t _readstatus[1] = {0x00};
        util_battery_status_s _status_converted;
        i2c_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _getstatus, 1, true, 10000);
        int readcheck = i2c_read_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _readstatus, 1, false, 10000);

        if ((readcheck == PICO_ERROR_GENERIC) || (readcheck == PICO_ERROR_TIMEOUT))
        {
            // No battery, must be USB
            return true;
        }

        _status_converted.status = _readstatus[0];
        return _status_converted.plug_status;
    #else
        printf("Battery capability is disabled. Returning connected.\n");
        return true;
    #endif
}