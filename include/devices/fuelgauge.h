#ifndef DEVICES_FUELGAUGE_H
#define DEVICES_FUELGAUGE_H
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool connected;  // If we have a fuel gauge connected or not
    uint8_t percent; // Percent 0-100
    uint8_t simple;  // 0-4 level (0: Empty, 1: Critical, 2: Low, 3: Med, 4: Full)
    bool discharge_only; // Set this flag if we only get accurate levels during discharge
} fuelgauge_status_s;

typedef enum 
{
    BATTERY_LEVEL_UNAVAILABLE = -1,
    BATTERY_LEVEL_CRITICAL,
    BATTERY_LEVEL_LOW,
    BATTERY_LEVEL_MID,
    BATTERY_LEVEL_HIGH
} battery_level_t;

void fuelgauge_update_status(void);
void fuelgauge_get_status(fuelgauge_status_s *out);
bool fuelgauge_init(uint16_t capacity_mah);

#endif