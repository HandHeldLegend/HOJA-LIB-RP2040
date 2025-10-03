#ifndef DEVICES_FUELGAUGE_H
#define DEVICES_FUELGAUGE_H
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool connected;  // If we have a fuel gauge connected or not
    uint8_t percent; // Percent 0-100
    uint8_t simple;  // 0-4 level (0: Empty, 1: Critical, 2: Low, 3: Med, 4: Full)
} fuelgauge_status_s;

void fuelgauge_get_status(fuelgauge_status_s *out);
void fuelgauge_set_connected(bool connected);
void fuelgauge_set_percent(uint8_t percent);

void fuelgauge_task(uint64_t timestamp);
bool fuelgauge_init(uint16_t capacity_mah);

#endif