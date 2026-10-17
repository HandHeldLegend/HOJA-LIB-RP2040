#ifndef UTILITIES_AUTODETECT_H
#define UTILITIES_AUTODETECT_H

// Auto mode: with nothing held at boot and the default mode set to Auto, the controller works out
// what it is plugged into.
//
// No external power (battery, or an N64, which supplies none): N64 first, answering as quickly as
// a manual N64 boot.
// External power: SInput over USB while watching the retro port. A host that asks for the BOS
// descriptor is a computer (SInput), one that does not is a Switch. Joybus traffic means a
// GameCube, latch pulses an SNES or NES.
// Nothing found (battery, or a charger): Bluetooth. A HOJA dongle first, on boards that reach it
// over Bluetooth: one that connects tells the controller the mode. Then each saved host is checked
// (Switch, then Wii, then SInput), and the first one awake picks the mode. With none awake it stays
// in Switch mode.
//
// Modes are switched in place, never with a restart. Every LED shows the player LED color until the
// mode is confirmed.

#include <stdint.h>
#include <stdbool.h>
#include "utilities/boot.h"

// gamepad_default_mode / gamepad_default_wireless value for Auto. Firmware without Auto reads it as
// Switch Pro.
#define GAMEPAD_DEFAULT_MODE_AUTO 0xFE

// gamepad_defaults_split once the default mode is split into wired and wireless
#define GAMEPAD_DEFAULTS_SPLIT 0x01

// From boot_init, when no mode was picked by hand and the default is Auto
typedef enum
{
    AUTODETECT_POWER_BATTERY,  // Running on its own battery
    AUTODETECT_POWER_EXTERNAL, // USB, a charger or a console powers it
    AUTODETECT_POWER_UNKNOWN,  // No PMIC to ask (boards without a battery)
} autodetect_power_t;

// wired_auto: the wired default is Auto (otherwise only the wireless search runs, on battery).
// wireless: the wireless default, CORE_REPORTFORMAT_UNDEFINED for Auto.
// Wireless always looks for a dongle first (boards with the dongle link), then goes to Bluetooth.
void autodetect_boot(boot_info_s *info, autodetect_power_t power, bool wired_auto, core_reportformat_t wireless);

// A wireless mode picked at power-on (a mode combo, the wireless default, pairing): the dongle first
// in that mode, then Bluetooth in it. Wii goes straight to Bluetooth.
void autodetect_wireless(boot_info_s *info, core_reportformat_t format);

// Core 1 checks this between task passes and brings up the new mode in place
bool autodetect_take_switch(core_reportformat_t *format, gamepad_transport_t *transport);
// Called once the new mode is up
void autodetect_switch_done(void);

bool autodetect_active(void);
// True until the detected mode is confirmed (keeps the status LED dark)
bool autodetect_pending(void);

void autodetect_task(uint64_t now_us);

// Events
void autodetect_on_connected(void);
void autodetect_on_usb_host(void);
void autodetect_on_usb_bos(void);

// The dongle connected while Auto mode looked for it (bluetooth_hal_floss.c)
void autodetect_dongle_found(void);

// Bluetooth probe (bluetooth_hal.c)
bool autodetect_bt_probe_pending(void);
// The mode of the host that answered, or CORE_REPORTFORMAT_UNDEFINED. True when Switch mode carries
// on, false when another mode takes over.
bool autodetect_bt_probe_result(core_reportformat_t found);

#endif
