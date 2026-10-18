#ifndef UTILITIES_PROFILES_H
#define UTILITIES_PROFILES_H

// Settings profiles (boards with the HOJA bootloader, whose top area holds the record store). A
// profile is how the player wants the controller to behave: notch and angle tables, deadzones and the
// stick curve, remaps, RGB, motion sensitivity and per-mode on/off, haptic strength, and its name.
// Everything that describes this physical controller stays global: calibration, identity, pairings,
// default modes, the user name.
//
// live_settings stays the one RAM view: the global settings with the active profile's parts in place.
// The app reads and writes blocks as always, editing the active profile; a save stores both. The
// app switches, renames and copies profiles over WebUSB (WEBUSB_ID_PROFILE); the controller itself
// has no way to switch.
//
// WEBUSB_ID_PROFILE packets: [id][op][profile][from (COPY)][name 24 (RENAME)], answered
// [id][op][1 done, 0 not][profile count, 0 without profiles][active profile][profile][name 24]
//   op 0 INFO    one profile's name (empty: the app names it)
//   op 1 SWITCH  the active profile (the one switched away from is saved first)
//   op 2 RENAME
//   op 3 COPY    from another profile into this one (it keeps its name)

#include <stdbool.h>
#include <stdint.h>

#define PROFILES_COUNT    4u
#define PROFILES_NAME_LEN 24u

typedef enum
{
    PROFILES_OP_INFO = 0,
    PROFILES_OP_SWITCH = 1,
    PROFILES_OP_RENAME = 2,
    PROFILES_OP_COPY = 3,
} profiles_op_t;

// From settings_init, once live_settings holds the old settings page: replaces it with the global
// record and the active profile, or (the first time) makes those from it. False without profiles.
bool profiles_init(void);

// settings_commit_blocks: true when the profiles store took the save (the global record and the
// active profile are written from the task)
bool profiles_save(void);

// From the task loop: writes one waiting record a pass
void profiles_task(uint64_t now_us);

// WEBUSB_ID_PROFILE
void profiles_webusb(const uint8_t *data, uint32_t size);

#endif
