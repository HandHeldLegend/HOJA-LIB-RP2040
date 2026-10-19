#ifndef UTILITIES_AUTODETECT_H
#define UTILITIES_AUTODETECT_H

// Auto mode: with nothing held at boot and the default mode set to Auto, the controller works out
// what it is plugged into.
//
// No external power (battery, or an N64, which supplies none): N64 first for a second, answering as
// quickly as a manual N64 boot. On battery the radio starts meanwhile.
// External power: SInput over USB while watching the retro port. A host that asks for the BOS
// descriptor is a computer (SInput), one that does not is a Switch. Joybus traffic means a
// GameCube, latch pulses an SNES or NES.
// Nothing found (battery, or a charger): Bluetooth. On boards that reach the HOJA dongle over
// Bluetooth, the controller first listens briefly for a free dongle's beacon: its remembered dongle,
// else the strongest free one (for this session only; only pairing remembers a new dongle), is
// called and tells the controller the mode. Otherwise the saved hosts are called straight away, in
// order (a Switch, then a Wii, then a PC), each in its own mode, skipping any never paired: the
// first that answers is connected. With none answering it settles on SInput, which keeps calling a
// saved PC or waits to be found. (BTstack boards probe the hosts first instead.)
//
// The config app (Start + Select) while Auto is still looking: on battery Auto settles on SInput at
// once; on external power it waits for the app, whose link a mode change would drop.
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

// A wireless start (Auto, a mode chosen for wireless, pairing) listens first for a free dongle that
// announces itself (its LE beacon, listened for by bluetooth_hal_floss.c). Listening while this is
// true; heard: that dongle is called, and a pairing start makes it the remembered one.
bool autodetect_dongle_listen(void);
void autodetect_dongle_listen_end(void);
void autodetect_dongle_heard(void);

// Pairing a Switch or a PC on battery: an open dongle first, then Bluetooth pairing in this mode
void autodetect_pairing(boot_info_s *info);

// Calling a saved host in its mode (bluetooth_hal_floss.c): whether it connected. One that didn't
// hands over to the next saved host.
bool autodetect_bt_calling(void);
void autodetect_bt_call_result(bool connected);

// Bluetooth probe (bluetooth_hal.c, BTstack boards until they move to FLOSS)
bool autodetect_bt_probe_pending(void);
// The mode of the host that answered, or CORE_REPORTFORMAT_UNDEFINED (none awake: SInput). True when
// Switch mode carries on, false when another mode takes over.
bool autodetect_bt_probe_result(core_reportformat_t found);

// --- The config app and pairing (bluetooth_hal_floss.c, macro_pairing.c) ---

// What powered the gamepad at boot, as Auto saw it
autodetect_power_t autodetect_power(void);

// Auto is still looking and no mode was chosen (a chosen mode may still be looking for its dongle)
bool autodetect_mode_open(void);

// The config app holds the radio: Auto's waits stop (each step past them restarts the radio under
// the app) and start over in full once it lets go
void autodetect_pause(bool paused);

// The config app's beacon opened while Auto is still looking, with no mode chosen and no external
// power: the app is the session, so Auto settles on SInput now. False otherwise (a cable may still
// bring a host, or a chosen mode carries on).
bool autodetect_settle_for_app(void);

#endif
