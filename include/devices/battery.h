#ifndef DEVICES_BATTERY_H
#define DEVICES_BATTERY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct 
{
    bool connected; // If we have a battery management IC connected (can also be used to disable function if left as false)
    bool charging;  // If we are charging or not
    bool charging_done; // Sets to true when charging is completed
    bool plugged;   // If we are plugged in to good power or not
} battery_status_s;

typedef enum
{
    BATTERY_SOURCE_AUTO,
    BATTERY_SOURCE_EXTERNAL,
    BATTERY_SOURCE_BATTERY,
} battery_source_t;

// Outcome of a battery operation. Battery functions are always safe to call;
// the return value tells the caller whether anything actually happened, so
// callers never need to guard on driver presence themselves.
typedef enum
{
    BATTERY_RESULT_NO_DRIVER = 0, // No battery driver assigned; nothing happened
    BATTERY_RESULT_OK,            // Operation completed successfully
    BATTERY_RESULT_FAILED,        // Driver present, but the operation failed
} battery_result_t;

// Forward declaration so the vtable can reference the instance type.
typedef struct battery_driver_s battery_driver_s;

// Operations every battery PMIC driver must implement.
// Each callback receives the owning driver instance so it can reach its
// driver-specific config via drv->cfg (no global state required).
typedef struct
{
    // Human-readable PMIC part number, supplied by the driver itself
    // (e.g. "BQ25180"). Surfaced to the config tool; not board-specific.
    const char *part_code;

    // init() also performs hardware presence detection: return false if the
    // PMIC is not responding. There is intentionally no separate is_present().
    bool             (*init)(const battery_driver_s *drv);
    battery_status_s (*get_status)(const battery_driver_s *drv);
    bool             (*set_charge_rate)(const battery_driver_s *drv, uint16_t rate_ma);
    bool             (*set_ship_mode)(const battery_driver_s *drv);
} battery_driver_api_s;

// A concrete driver instance: a vtable plus a pointer to the driver's own
// configuration struct. Boards declare one of these (const, so it lives in
// flash) and inject it through the hoja config.
struct battery_driver_s
{
    const battery_driver_api_s *api;
    const void                 *cfg;
};

void battery_update_status(void);
void battery_get_status(battery_status_s *out); 
void battery_set_critical_shutdown(void); 
battery_result_t battery_init(void); 
battery_result_t battery_set_charge_rate(uint16_t rate_ma); 
battery_result_t battery_set_ship_mode(void); 

#endif