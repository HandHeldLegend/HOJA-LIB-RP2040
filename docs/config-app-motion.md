# Config app update: motion flicks and per-mode motion

Paste this into a session working on the HOJA config app (hoja3). It describes the firmware's new
motion flick outputs (Switch and Wii) and the per-mode motion switch in the IMU block. All
multi-byte values are little-endian and all structs are packed. Every change is append-only or
uses reserved space, so no block changed size and older firmware and app builds stay compatible.

## What the flicks are

A flick output replays a real flick (recorded on a GCU-2 and averaged) once per press on the
reported accelerometer and gyro,
for games that read motion with no way to press a button instead. Holding the input does not
repeat it; press again to flick again.

| Gesture | What the host sees |
| --- | --- |
| Flick Up | A sharp upward push, wrist turning nose-up |
| Flick Down | A sharp downward push, wrist turning nose-down |
| Flick Left | A sharp push to the left, turning left |
| Flick Right | A sharp push to the right, turning right |

Games that only check for a shake (most Wii games, Kirby, party games) take a flick in any
direction. Super Mario Odyssey reads the direction even on a Pro Controller: Flick Up and Flick
Down give the upward and downward cap throws.

Flicks are added to the raw motion samples, so hosts process them exactly like real motion: they
show in both Switch motion formats (raw and quaternion), go through each Wii mode's remote
mapping, and nudge the Wii pointer the way a real flick would (each flick ends where it started).
They also play while motion is turned off or on boards with no IMU: the controller then reads as
lying flat on a table, plus the flick.

Flicks follow the ground: with motion on, each flick is turned to match how the controller is
held when it starts (from the accelerometer's gravity reading), so Flick Up always moves away
from the ground, even with the controller on its side. Gravity cannot tell which way the
controller faces, so left and right stay level toward the controller's own sides. In Wii Classic mode the remote sits idle, so Remote flicks
do nothing there.

## 1. Switch output codes (`mapper_switch_code_t`)

Appended after `SWITCH_CODE_RY_DOWN` (25):

| Value | Code | Label | Type |
| --- | --- | --- | --- |
| 26 | `SWITCH_CODE_FLICK_UP` | Flick Up | Digital |
| 27 | `SWITCH_CODE_FLICK_DOWN` | Flick Down | Digital |
| 28 | `SWITCH_CODE_FLICK_LEFT` | Flick Left | Digital |
| 29 | `SWITCH_CODE_FLICK_RIGHT` | Flick Right | Digital |

They use the existing `input_profile_switch` slots; no layout change. Suggested picker group:
**Motion** (26–29).

## 2. Wii output codes (`mapper_wii_code_t`)

The Remote and the Nunchuk each get the four flicks (the Nunchuk has no gyro, so its flicks are
accelerometer only). They replace Shake Remote and Shake Nunchuk, and Extension Attach/Detach
moved from 47 to 45. Wii mode has not shipped yet, so there are no saved profiles to migrate:

| Value | Code | Label | Type | Used by |
| --- | --- | --- | --- | --- |
| 45 | `WII_CODE_EXTENSION_TOGGLE` | Extension Attach/Detach | Digital | all |
| 46 | `WII_CODE_REMOTE_FLICK_UP` | Remote Flick Up | Digital | all |
| 47 | `WII_CODE_REMOTE_FLICK_DOWN` | Remote Flick Down | Digital | all |
| 48 | `WII_CODE_REMOTE_FLICK_LEFT` | Remote Flick Left | Digital | all |
| 49 | `WII_CODE_REMOTE_FLICK_RIGHT` | Remote Flick Right | Digital | all |
| 50 | `WII_CODE_NUNCHUK_FLICK_UP` | Nunchuk Flick Up | Digital | Nunchuk |
| 51 | `WII_CODE_NUNCHUK_FLICK_DOWN` | Nunchuk Flick Down | Digital | Nunchuk |
| 52 | `WII_CODE_NUNCHUK_FLICK_LEFT` | Nunchuk Flick Left | Digital | Nunchuk |
| 53 | `WII_CODE_NUNCHUK_FLICK_RIGHT` | Nunchuk Flick Right | Digital | Nunchuk |

Wii defaults are still derived from the Switch defaults. Where the Wii layouts used Shake Remote
(R, and ZR in Sideways) they now use Remote Flick Up, and LS click in Upright is Nunchuk Flick
Up. A Switch flick binding becomes the matching Remote flick.

## 3. IMU config block: per-mode motion switch

`imuConfig_s` stays 32 bytes. `imu_config_version` goes from 0x12 to 0x13, and two reserved
bytes become a mask:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | `imu_config_version` (**0x13**) |
| 1 | 12 | gyro offsets / accel config (unchanged) |
| 13 | 1 | `imu_disabled` (unchanged: 1 = motion off in every mode) |
| 14 | 3 | `imu_gyro_sensitivity` (unchanged) |
| 17 | 3 | `imu_accel_sensitivity` (unchanged) |
| **20** | 2 | **`imu_mode_disable_mask`** (uint16) |
| 22 | 10 | `reserved` (was 12 bytes at offset 20) |

Bit *n* of `imu_mode_disable_mask` set = motion off in report format *n* (`core_reportformat_t`):
bit 0 Switch, bit 6 SInput, bit 7 Wii. Zero means motion on everywhere. `imu_disabled` stays the
master switch: when it is 1 motion is off in every mode whatever the mask says.

Motion is on for a mode when the board has an IMU, `imu_disabled` is 0 and the mode's bit is
clear. While it is off the mode reports a controller lying still (SInput does not advertise
motion at all), and the flick outputs still work.

Firmware migrates a 0x12 block to 0x13 in place: calibration, sensitivity and `imu_disabled` are
kept and the mask starts at 0. Only read the mask when the version is 0x13 or later; on older
firmware hide the per-mode toggles.

## 4. Unknown codes

Firmware treats any output code it does not know as unmapped. A profile written by this app with
Switch codes 26–29 is safe to load on older firmware; those bindings just do
nothing there. No board ships flick codes in its Switch defaults, so an app that predates this
change only meets them if they were bound with this one.

## 5. Suggested UI

- Add a **Motion** group to the Switch and Wii output pickers with the codes above (Wii: Remote
  flicks with the pointer outputs, Nunchuk flicks with the Nunchuk outputs).
- In the IMU section, keep the existing motion on/off switch as "Motion (all modes)" and add
  per-mode toggles under it for Switch, SInput and Wii, greyed out while the master switch is off.
  Note that the flick buttons keep working with motion off.
- Help text: most games only check for a shake, so any flick works; Super Mario Odyssey reads
  the direction, with Flick Up / Down for the upward and downward cap throws.
