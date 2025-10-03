#ifndef DEVICES_FUELGAUGE_H
#define DEVICES_FUELGAUGE_H
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool connected;  // If we have a fuel gauge connected or not
    uint8_t percent; // Percent 0-100
} fuelgauge_status_s;

fuelgauge_status_s fuelgauge_get_status(void);
void fuelgauge_set_connected(bool connected);
void fuelgauge_set_percent(uint8_t percent);

void fuelgauge_task(uint64_t timestamp);
void fuelgauge_init();

#endif