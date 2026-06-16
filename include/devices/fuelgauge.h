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

// Outcome of a fuel gauge operation. Fuel gauge functions are always safe to
// call; the return value reports whether anything actually happened.
typedef enum
{
    FUELGAUGE_RESULT_NO_DRIVER = 0, // No fuel gauge driver assigned
    FUELGAUGE_RESULT_OK,            // Operation completed successfully
    FUELGAUGE_RESULT_FAILED,        // Driver present, but the operation failed
} fuelgauge_result_t;

// Forward declaration so the vtable can reference the instance type.
typedef struct fuelgauge_driver_s fuelgauge_driver_s;

// Operations every fuel gauge driver must implement. Each callback receives
// the owning driver instance so it can reach its driver-specific config via
// drv->cfg. init() also performs presence detection (no separate is_present).
typedef struct
{
    // Human-readable fuel gauge part number, supplied by the driver itself
    // (e.g. "BQ27621G1"). Surfaced to the config tool; not board-specific.
    const char *part_code;

    bool               (*init)(const fuelgauge_driver_s *drv, uint16_t capacity_mah);
    fuelgauge_status_s (*get_status)(const fuelgauge_driver_s *drv);
} fuelgauge_driver_api_s;

// A concrete driver instance: a vtable plus a pointer to the driver's own
// configuration struct. Boards declare one (const) and inject it through the
// hoja config.
struct fuelgauge_driver_s
{
    const fuelgauge_driver_api_s *api;
    const void                   *cfg;
};

void fuelgauge_update_status(void);
void fuelgauge_get_status(fuelgauge_status_s *out);
fuelgauge_result_t fuelgauge_init(void);

#endif