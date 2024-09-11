#ifndef BATTERY_H
#define BATTERY_H

#include "hoja_includes.h"
// Documentation: https://www.ti.com/lit/ds/symlink/bq25180.pdf

// Each type corresponds to the battery I2C address
typedef enum
{
    BATTYPE_UNDEFINED = 0x00,
    BATTYPE_BQ25180 = 0x6A,
} util_battery_type_t;

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
} __attribute__ ((packed)) BQ25180_status_s;

typedef enum 
{
    PMIC_SOURCE_AUTO,
    PMIC_SOURCE_EXT,
    PMIC_SOURCE_BAT,
    PMIC_SOURCE_MAX,
} BQ25180_battery_source_t;

uint16_t battery_get_level();
void battery_update_status();
void battery_monitor_task(uint32_t timestamp);
bool battery_comms_check();
void battery_enable_ship_mode();
void battery_set_charge_rate(uint16_t rate_ma);
void battery_set_source(BQ25180_battery_source_t source);
int8_t battery_get_plugged_status();

#endif