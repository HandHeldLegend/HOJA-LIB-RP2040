#ifndef BATTERY_H
#define BATTERY_H

#include "hoja_includes.h"

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
} __attribute__ ((packed)) util_battery_status_s;

bool util_battery_comms_check();
void util_battery_monitor_task_usb(uint32_t timestamp);
void util_battery_monitor_task_wireless(uint32_t timestamp);

void util_battery_enable_ship_mode(void);
void util_battery_set_charge_rate(uint16_t rate_ma);
bool util_wire_connected();

#endif