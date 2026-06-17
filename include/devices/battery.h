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

// ---- Battery driver contract (weak-function model) ----
// The selected battery driver provides strong definitions of these. Which
// driver compiles is decided by the HOJA_BATTERY_DRIVER gate in board_config.h.
// battery.c ships weak defaults so that when no driver is selected every call
// is a safe no-op. The driver reads its own configuration straight from the
// hoja config (hoja_config_get()->battery), whose type is shaped by the gate.
//
// battery_driver_part_code() returning NULL is the canonical "no driver
// present" signal used by the device layer; a real driver returns its PMIC
// part number (e.g. "BQ25180"). init() also performs hardware presence
// detection and returns false if the PMIC is not responding.
bool             battery_driver_init(void);
battery_status_s battery_driver_get_status(void);
bool             battery_driver_set_charge_rate(uint16_t rate_ma);
bool             battery_driver_set_ship_mode(void);
const char      *battery_driver_part_code(void);

void battery_update_status(void);
void battery_get_status(battery_status_s *out); 
void battery_set_critical_shutdown(void); 
battery_result_t battery_init(void); 
battery_result_t battery_set_charge_rate(uint16_t rate_ma); 
battery_result_t battery_set_ship_mode(void); 

#endif