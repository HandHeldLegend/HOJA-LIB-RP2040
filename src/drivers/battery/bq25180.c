#include "drivers/battery/bq25180.h"
#include "hal/i2c_hal.h"

#define BQ25180_SLAVE_ADDRESS 0x6A

typedef struct
{
    union
    {
        struct 
        { 
            uint8_t plug_status : 1;
            uint8_t dummy1 : 4;
            uint8_t charge_status : 2;
            uint8_t dummy2 : 1;
        };
        uint8_t status;
    };
} bq25180_status_s;

bool _bq25180_comms_check()
{
    uint8_t _getstatus[1] = {0x00};
    uint8_t _readstatus[1] = {0x00};
    int ret = i2c_hal_write_read_timeout_us(HOJA_BATTERY_I2C_INSTANCE, BQ25180_SLAVE_ADDRESS, _getstatus, 1, _readstatus, 1, 32000);

    if(ret==1)
    {
        return true;
    }
    // Communication failure with PMIC
    else return false;
}

bool bq25180_init()
{
    if(_bq25180_comms_check())
    {
        bq25180_set_source(BATTERY_SOURCE_AUTO);
    }
}

battery_status_s bq25180_update_status()
{

}

int  bq25180_get_level()
{

}

bool bq25180_set_source(battery_source_t source)
{
    // We want to disable the onboard regulation
    // SYS_REG_CTRL_2:0 set as 111 will set as Pass-Through 
    uint8_t source = 0b11100000;

    // We will enable the I2C watchdog.
    // This ensures that if there's a freeze or lockup, we will reboot.
    source |= 0b10;

    // Disable VDPPM (set VPPM_DIS bit) to allow charger operation with lower voltage input.
    source |= 0b1;

    switch(source)
    {   
        case BATTERY_SOURCE_EXTERNAL: // Picking external only isn't supported.
        case BATTERY_SOURCE_AUTO:
        source |= 0b0000; // Automatically choose Battery or VIN
        break;

        case BATTERY_SOURCE_BATTERY:
        source |= 0b0100; // ONLY power from Battery
        break;
    }

    uint8_t write[2] = {0xA, source};

    int ret = i2c_hal_write_blocking(HOJA_BATTERY_I2C_INSTANCE, BQ25180_SLAVE_ADDRESS, write, 2, false);
}

bool bq25180_set_ship_mode()
{

}

bool bq25180_set_charge_rate(uint16_t rate_ma)
{
    // Default is 0x5
    uint8_t code = 0x5;
    uint16_t rate = rate_ma;

    uint8_t ich_ctrl = 0x4;

    uint8_t write[2] = {ich_ctrl, code};

    if(!rate_ma) // Charging disabled
    {
        write[1] = 0b10000101;

        int ret = i2c_hal_write_blocking(HOJA_BATTERY_I2C_INSTANCE, BQ25180_SLAVE_ADDRESS, write, 2, false);
        if(ret==2) 
        {
            return true;
        }
        else 
        {
            return false;
        }
    }
    else
    {
        if (rate_ma <= 35)
        {
            // CODE + 5 = rate
            code = rate - 5;
        }
        else
        {
            if (rate > 350) // Safety cap at 350mA charge rate
            {
                rate = 350;
            }

            // 40 + ((CODE-31)*10) = rate
            code = ((rate - 40) / 10) + 31;
            write[1] = code;

            int ret = i2c_hal_write_blocking(HOJA_BATTERY_I2C_INSTANCE, BQ25180_SLAVE_ADDRESS, write, 2, false);
            if(ret==2) 
            {
                return true;
            }
            else 
            {
                return false;
            }
            
        }
    }

    return false;
}
