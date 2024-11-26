#ifndef DRIVERS_BATTERY_BQ25180_H
#define DRIVERS_BATTERY_BQ25180_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

void driver_battery_enable_shipmode();

bool driver_battery_comms_check();

void driver_battery_update_status();

void driver_battery_current_status();

uint16_t driver_battery_get_level();

#endif