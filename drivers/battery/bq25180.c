#include "drivers/battery/bq25180.h"

#define BQ25180_SLAVE_ADDRESS 0x6A

typedef enum
{
    POWER_SOURCE_AUTO,
    POWER_SOURCE_EXTERNAL,
    POWER_SOURCE_BATTERY,
    POWER_SOURCE_MAX,
}   driver_BatterySource_t;

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
} BQ25180_Status_s;

// Test the status and function
// of the battery hardware and return an appropriate value
int battery_driver_hwtest()
{

}

void battery_driver_enable_shipmode()
{

}

void battery_driver_set_source(driver_BatterySource_t source)
{

}

bool battery_driver_comms_check()
{

}

void battery_driver_set_charge_rate(uint16_t rate_ma)
{

}

void battery_driver_update_status()
{

}

void battery_driver_current_status()
{

}

uint16_t battery_driver_get_level()
{

}