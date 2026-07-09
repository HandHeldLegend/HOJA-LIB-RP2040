#ifndef DEVICES_FUELGAUGE_H
#define DEVICES_FUELGAUGE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

// Outcome of a fuel gauge operation. Fuel gauge functions are always safe to
// call; the return value reports whether anything actually happened.
typedef enum
{
    FUELGAUGE_RESULT_NO_DRIVER = 0, // No fuel gauge driver assigned
    FUELGAUGE_RESULT_OK,            // Operation completed successfully
    FUELGAUGE_RESULT_FAILED,        // Driver present, but the operation failed
} fuelgauge_result_t;

// ---- Fuel gauge driver contract (weak-function model) ----
// The selected fuel gauge driver provides strong definitions of these. Which
// driver compiles is decided by the HOJA_FUELGAUGE_DRIVER gate in
// board_config.h. fuelgauge.c ships weak defaults so that when no driver is
// selected every call is a safe no-op. The driver reads its own configuration
// straight from the hoja config (hoja_config_get()->fuelgauge), whose type is
// shaped by the gate (drivers without config, e.g. ESP32, read nothing).
//
// fuelgauge_driver_part_code() returning NULL is the canonical "no driver
// present" signal used by the device layer; a real driver returns its part
// number (e.g. "BQ27621G1"). init() also performs hardware presence detection.
bool               fuelgauge_driver_init(uint16_t capacity_mah);
fuelgauge_status_s fuelgauge_driver_get_status(void);
const char        *fuelgauge_driver_part_code(void);

// True when a board fuel gauge driver is compiled in for this target.
static inline bool fuelgauge_has_driver(void)
{
    return fuelgauge_driver_part_code() != NULL;
}

void fuelgauge_update_status(void);
void fuelgauge_get_status(fuelgauge_status_s *out);
fuelgauge_result_t fuelgauge_init(void);

#endif