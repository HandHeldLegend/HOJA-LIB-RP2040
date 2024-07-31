#include "battery.h"
#include "interval.h"

util_battery_status_s _util_battery_status = {0};

void util_battery_init()
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
    const uint8_t _data[2] = {0x09, 0x41};
    int s = i2c_safe_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _data, 2, false, 10000);
    #endif
}

// Battery monitor task we run when we are in a wired mode
void util_battery_monitor_task_usb(uint32_t timestamp)
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
    static uint8_t charging = 0;
    static interval_s interval = {0};
    // Check status every 64ms
    if(interval_run(timestamp, 64000, &interval))
    {
        static bool _connected = true;
        _connected = util_wire_connected();
        if(!_connected)
        {
            // Not the cause
            hoja_shutdown();
        }

        static bool idle = false;

        if(!idle && hoja_get_idle_state())
        {
            charging = 0;
            idle = true;
        }
        else if(idle && !hoja_get_idle_state())
        {
            idle = false;
            rgb_init(global_loaded_settings.rgb_mode, -1);
        }

        if(idle)
        {
            if(charging != _util_battery_status.charge_status)
            {
                rgb_s color = COLOR_YELLOW;
                switch(_util_battery_status.charge_status)
                {
                    default:
                    case 0b00:
                        color = COLOR_RED;
                        break;
                    case 0b01:
                    case 0b10:
                        color = COLOR_YELLOW;
                        break; 
                    case 0b11:
                        color = COLOR_BLUE;
                        break;
                }
                charging = _util_battery_status.charge_status;
                rgb_flash(color.color, 100);
            }
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
        int readcheck = i2c_safe_write_read_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _getstatus, 1, _readstatus, 1, 10000);
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
    static interval_s interval = {0};

    // Check status every 64ms
    if(interval_run(timestamp, 64000, &interval))
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
    int s1 = 0;
    while(!s2)
    {

        //const uint8_t _data1[2] = {0x0A, 0b01001100};
        //s1 = i2c_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _data1, 2, false, 10000);

        const uint8_t _data[2] = {0x09, 0x41};
        s2 = i2c_safe_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _data, 2, false, 10000);

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

uint16_t _util_charge_rate = 0;

void util_battery_set_charge_rate(uint16_t rate_ma)
{
    _util_charge_rate = rate_ma;
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

    if(!rate_ma)
    {
        code = 0b10000101;
    }

    uint8_t _write[2] = {0x04, code};
    
    #if (HOJA_CAPABILITY_BATTERY == 1)
    int readcheck = i2c_safe_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _write, 2, false, 10000);
    #endif
}

uint8_t util_battery_get_level()
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
    uint16_t store_rate = _util_charge_rate;
    util_battery_set_charge_rate(0);

    uint8_t rate = cb_hoja_get_battery_level();

    util_battery_set_charge_rate(store_rate);
    return rate;
    #else
    return 100;
    #endif
}

// This disables the PMIC regulation by default. We already have a regulator
void util_battery_set_source(util_battery_source_t source)
{
    #if (HOJA_CAPABILITY_BATTERY == 1)

    uint8_t source_packet = 0b11100000;
    switch(source)
    {
        default:
        case PMIC_SOURCE_AUTO:
        case PMIC_SOURCE_EXT:
        break;
        case PMIC_SOURCE_BAT:
            source_packet |= 0b00000100;
        break;
    }
    const uint8_t _data[2] = {0x0A, source_packet};
    int success = i2c_safe_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _data, 2, false, 10000);

    if(success == PICO_ERROR_GENERIC)
    {
        
    }
    else if (success == PICO_ERROR_TIMEOUT)
    {
        
    }
    #endif 
}

bool util_wire_connected()
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
        // We can poll the PMIC to get our plug status
        uint8_t _getstatus[1] = {0x00};
        uint8_t _readstatus[1] = {0x00};
        
        int readcheck = i2c_safe_write_read_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _getstatus, 1, _readstatus, 1, 10000);

        if ((readcheck == PICO_ERROR_GENERIC) || (readcheck == PICO_ERROR_TIMEOUT))
        {
            // No battery, must be USB
            return true;
        }

        _util_battery_status.status = _readstatus[0];
        return _util_battery_status.plug_status;
    #else
        printf("Battery capability is disabled. Returning connected.\n");
        return true;
    #endif
}