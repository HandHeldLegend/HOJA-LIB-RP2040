#include "drivers/battery/bq25180.h"
#include "hal/i2c_hal.h"
#include "hal/sys_hal.h"
#include "hal/mutex_hal.h"

#define BQ25180_SLAVE_ADDRESS 0x6A

#define BQ25180_REG_STATUS_0    0x0
#define BQ25180_REG_STATUS_1    0x1
#define BQ25180_REG_SHIP_RST    0x9
#define BQ25180_REG_SYS_REG     0xA
#define BQ25180_REG_ICHG_CTRL   0x4
#define BQ25180_REG_MASK_ID     0xC

typedef struct
{
    union
    {
        struct 
        { 
            uint8_t vin_power_good : 1;
            uint8_t thermal_regulation_active : 1;
            uint8_t vindpm_mode_active : 1;
            uint8_t vdppm_active_stat : 1;
            uint8_t ilim_active_stat : 1;
            uint8_t charge_stat : 2;
            uint8_t ts_open_stat : 1;
        };
        uint8_t status;
    };
} bq25180_status_0_s;

typedef struct
{
    union
    {
        struct 
        { 
            uint8_t wake2_flag : 1;
            uint8_t wake1_flag : 1;
            uint8_t safety_tmr_fault_flag : 1;
            uint8_t ts_status : 2;
            uint8_t reserved : 1;
            uint8_t buvlo_status : 1;
            uint8_t vin_ovp_status : 1;
        };
        uint8_t status;
    };
} bq25180_status_1_s;

static bool _charge_disabled = false;

// Pull the I2C bus instance out of the injected driver config.
static inline uint8_t _bus(const battery_driver_s *drv)
{
    return ((const bq25180_cfg_s *)drv->cfg)->i2c_instance;
}

// Hardware presence detection (internal). Folded into init() rather than
// exposed as a separate vtable entry.
static bool bq25180_is_present(const battery_driver_s *drv)
{
    uint8_t i2c = _bus(drv);

    uint8_t _getstatus[1] = {BQ25180_REG_MASK_ID};
    uint8_t _readstatus[1] = {0x00};
    int ret = i2c_hal_write_read_timeout_us(i2c, BQ25180_SLAVE_ADDRESS, _getstatus, 1, _readstatus, 1, 32000);

    if(ret==1)
    {
        return true;
    }
    // Communication failure with PMIC
    else return false;
}

static battery_status_s bq25180_get_status(const battery_driver_s *drv)
{
    uint8_t i2c = _bus(drv);

    static battery_status_s status = {0};

    bq25180_status_0_s this_status_0 = {0};
    bq25180_status_1_s this_status_1 = {0};

    uint8_t _getstatus[1] = {BQ25180_REG_STATUS_0};
    uint8_t _readstatus[1] = {0};
    int ret = i2c_hal_write_read_timeout_us(i2c, BQ25180_SLAVE_ADDRESS, _getstatus, 1, _readstatus, 1, 1000);

    if(ret==1)
    {
        status.connected = true;

        this_status_0.status = _readstatus[0];
        
        // First, get our plug status
        if(this_status_0.vin_power_good)
        {
            status.plugged = true;
        }
        else
        {
            status.plugged = false;
        }

        // Process any changes to status
        switch(this_status_0.charge_stat)
        {
            case 0b00:
                // We only get this status while charging is enabled
                status.charging = false;
            break;

            case 0b01:
            case 0b10:
                // Reset charging_done
                status.charging_done = false;
                status.charging = true;
            break;

            case 0b11:
                if(_charge_disabled)
                {
                    status.charging = false;
                }
                else
                {
                    status.charging_done = true;
                    status.charging = false;
                }
            break;
        }   
    }
    
    return status;
}

static bool bq25180_set_source(const battery_driver_s *drv, battery_source_t source)
{
    uint8_t i2c = _bus(drv);

    // Disable pushbutton

    // Broken code? Causes issues with wired only with 3.3v sources
    const uint8_t write1[2] = {BQ25180_REG_SHIP_RST, 0b00000000}; // Ship mode with wake on button press/adapter insert
    int ret1 = i2c_hal_write_blocking(i2c, BQ25180_SLAVE_ADDRESS, write1, 2, false);


    // We want to disable the onboard regulation
    // SYS_REG_CTRL_2:0 set as 111 will set as Pass-Through 
    uint8_t new_source = 0b11100000;

    // We will enable the I2C watchdog.
    // This ensures that if there's a freeze or lockup, we will reboot.
    //new_source |= 0b10;

    // Disable VDPPM (set VPPM_DIS bit) to allow charger operation with lower voltage input.
    // 15 seconds is the timeout
    new_source |= 0b1;

    switch(source)
    {   
        case BATTERY_SOURCE_EXTERNAL: // Picking external only isn't supported.
        case BATTERY_SOURCE_AUTO:
        new_source |= 0b0000; // Automatically choose Battery or VIN
        break;

        case BATTERY_SOURCE_BATTERY:
        new_source |= 0b0100; // ONLY power from Battery
        break;
    }

    uint8_t write[2] = {BQ25180_REG_SYS_REG, new_source};

    int ret = i2c_hal_write_blocking(i2c, BQ25180_SLAVE_ADDRESS, write, 2, false);

    if(ret==2) return true; 

    return false;
}

static bool bq25180_set_charge_rate(const battery_driver_s *drv, uint16_t rate_ma)
{
    uint8_t i2c = _bus(drv);

    // Default is 0x5
    uint8_t code = 0x5;
    uint16_t rate = rate_ma;

    uint8_t write[2] = {BQ25180_REG_ICHG_CTRL, code};

    if(!rate_ma) // Charging disabled
    {
        _charge_disabled = true;
        write[1] = 0b10000101;

        int ret = i2c_hal_write_blocking(i2c, BQ25180_SLAVE_ADDRESS, write, 2, false);
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
        _charge_disabled = false;
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

            int ret = i2c_hal_write_blocking(i2c, BQ25180_SLAVE_ADDRESS, write, 2, false);
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

    // Should never reach here... 
    return false;
}

static bool bq25180_set_ship_mode(const battery_driver_s *drv)
{
    uint8_t i2c = _bus(drv);

    const uint8_t write[2] = {BQ25180_REG_SHIP_RST, 0b01000001}; // Ship mode with wake on button press/adapter insert
    int ret = i2c_hal_write_timeout_us(i2c, BQ25180_SLAVE_ADDRESS, write, 2, false, 16000);

    if(ret == PICO_ERROR_GENERIC)
    {
        //hoja_shutdown_instant();
    }
    else if (ret== PICO_ERROR_TIMEOUT)
    {
        //hoja_shutdown_instant();
    }
    else if (ret==2)
    {
    }

    return (ret==2);
}

static bool bq25180_init(const battery_driver_s *drv)
{
    const bq25180_cfg_s *cfg = (const bq25180_cfg_s *)drv->cfg;

    // Bail out if the PMIC isn't responding on the bus.
    if(!bq25180_is_present(drv)) return false;

    sys_hal_sleep_ms(100);
    bq25180_set_source(drv, BATTERY_SOURCE_AUTO);
    bq25180_set_charge_rate(drv, cfg->charge_rate_ma);
    return true;
}

const battery_driver_api_s bq25180_battery_api = {
    .part_code       = "BQ25180",
    .init            = bq25180_init,
    .get_status      = bq25180_get_status,
    .set_charge_rate = bq25180_set_charge_rate,
    .set_ship_mode   = bq25180_set_ship_mode,
};
