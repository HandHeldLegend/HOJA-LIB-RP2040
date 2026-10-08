# Config app update: Wii console mode

Paste this into a session working on the HOJA config app (hoja3). It describes everything the
firmware added for Wii mode that the app needs to read, display or edit. All multi-byte values are
little-endian, all structs are packed, and every change is append-only or uses space that was
reserved, so older firmware and older app builds stay compatible.

## What Wii mode is

Boards with the Raspberry Pi RM2 (CYW43) Bluetooth radio can now act as a Wii Remote over
Bluetooth. Holding **d-pad up** at boot enters Wii mode (alongside d-pad left / down / right for
SNES / N64 / GameCube). The status LED is pink. The controller has three Wii controller modes, and a
short tap of the power button cycles them (the LED flashes white / yellow / blue):

| Mode | What the Wii sees | Input profile |
| --- | --- | --- |
| Upright | Wii Remote held upright, Nunchuk attached | Wii Nunchuk profile |
| Sideways | Wii Remote held sideways, no extension | Wii Sideways profile |
| Classic | Wii Remote + the original Classic Controller (analog L/R) | Wii Classic profile |

The extension toggle (Capture by default) plugs or unplugs the current mode's extension at any
time, for games that ask for it to be removed: the Nunchuk in Upright and Sideways, the Classic
Controller in Classic. The LED flashes green when attached and red when detached.

In every mode the IR pointer follows the gyro, can be nudged by whatever is mapped to the pointer
outputs, and can be recentred by whatever is mapped to Pointer Recenter. Aiming works in any grip
(flat, rolled, or pointed straight up or down), the cursor tilts with the controller, and
recentring also makes the current pose "level" for the remote's reported tilt.

The controller reports as a Wii Remote Plus (MotionPlus built in), so MotionPlus games such as
Wii Sports Resort run. None of this needs settings; it is listed here for help text.

## 1. Capability: does this board support Wii mode?

`bluetoothInfoStatic_s` (the Bluetooth static info block) grew by one byte, from 55 to 56 bytes:

| Offset | Size | Field | Meaning |
| --- | --- | --- | --- |
| 55 | 1 | `wii_supported` | 1 when Wii mode is available, 0 otherwise |

Older firmware sends 55 bytes, so treat a missing byte as 0. Only show Wii options when this is 1.

## 2. Report format / default mode value

`core_reportformat_t` gained `CORE_REPORTFORMAT_WII = 7`.

`gamepadConfig_s.gamepad_default_mode` (byte 1 of the gamepad config block) may now be 7 = Wii.
Offer "Wii" in the default-mode picker only when `wii_supported` is 1. Firmware falls back to
Switch if the value is not supported on that board.

## 3. Gamepad config block: saved Wii address

`gamepadConfig_s` (64 bytes) took 6 bytes from its reserved tail:

| Offset | Size | Field |
| --- | --- | --- |
| 39 | 6 | `host_mac_wii` (address of the paired Wii) |
| 45 | 19 | `reserved` (was 25 bytes at offset 39) |

Read-only for the app; display it next to the Switch / SInput host addresses if those are shown.
Writing zeros forgets the paired Wii.

## 4. Input config block: three new profiles

`inputConfig_s` stays 2048 bytes. The former `input_profile_reserved_2` and part of the reserved
tail are now the Wii profiles. Each profile is 36 slots of `inputConfigSlot_s` (5 bytes each,
180 bytes), in the same format as every other profile.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | `input_config_version` (unchanged, 0x14) |
| 1 | 180 | `input_profile_switch` |
| 181 | 180 | `input_profile_xinput` |
| 361 | 180 | `input_profile_snes` |
| 541 | 180 | `input_profile_n64` |
| 721 | 180 | `input_profile_gamecube` |
| 901 | 180 | `input_profile_sinput` |
| **1081** | 180 | **`input_profile_wii_nunchuk`** (was `input_profile_reserved_2`) |
| **1261** | 180 | **`input_profile_wii_classic`** |
| **1441** | 180 | **`input_profile_wii_sideways`** |
| **1621** | 1 | **`wii_profile_version`** (0x03 once the firmware filled the Wii defaults) |
| 1622 | 426 | `reserved` |

Slot layout (unchanged): `uint16 output_mode:3, static_output:13; uint16 threshold_delta;
int8 output_code`. `output_code` is a `WII_CODE_*` value (below) for all three Wii profiles.
The firmware fills Wii defaults on first boot by translating the board's Switch defaults; the app
does not need to seed them.

## 5. Wii output codes (`mapper_wii_code_t`)

One code space is shared by all three Wii profiles. `-1` is unused. "Type" is the mapper output
type, which decides which output modes (threshold / rapid / passthrough, static output) apply,
exactly as for other profiles.

| Value | Code | Label | Type | Used by |
| --- | --- | --- | --- | --- |
| 0 | `WII_CODE_A` | Remote A | Digital | all |
| 1 | `WII_CODE_B` | Remote B | Digital | all |
| 2 | `WII_CODE_ONE` | Remote 1 | Digital | all |
| 3 | `WII_CODE_TWO` | Remote 2 | Digital | all |
| 4 | `WII_CODE_UP` | Remote Up | D-pad | all |
| 5 | `WII_CODE_DOWN` | Remote Down | D-pad | all |
| 6 | `WII_CODE_LEFT` | Remote Left | D-pad | all |
| 7 | `WII_CODE_RIGHT` | Remote Right | D-pad | all |
| 8 | `WII_CODE_PLUS` | Remote + | Digital | all |
| 9 | `WII_CODE_MINUS` | Remote − | Digital | all |
| 10 | `WII_CODE_HOME` | Remote Home | Digital | all |
| 11 | `WII_CODE_C` | Nunchuk C | Digital | Nunchuk |
| 12 | `WII_CODE_Z` | Nunchuk Z | Digital | Nunchuk |
| 13 | `WII_CODE_NUNCHUK_X_RIGHT` | Nunchuk Stick Right | Joystick | Nunchuk |
| 14 | `WII_CODE_NUNCHUK_X_LEFT` | Nunchuk Stick Left | Joystick | Nunchuk |
| 15 | `WII_CODE_NUNCHUK_Y_UP` | Nunchuk Stick Up | Joystick | Nunchuk |
| 16 | `WII_CODE_NUNCHUK_Y_DOWN` | Nunchuk Stick Down | Joystick | Nunchuk |
| 17 | `WII_CODE_CC_A` | Classic A | Digital | Classic |
| 18 | `WII_CODE_CC_B` | Classic B | Digital | Classic |
| 19 | `WII_CODE_CC_X` | Classic X | Digital | Classic |
| 20 | `WII_CODE_CC_Y` | Classic Y | Digital | Classic |
| 21 | `WII_CODE_CC_UP` | Classic Up | D-pad | Classic |
| 22 | `WII_CODE_CC_DOWN` | Classic Down | D-pad | Classic |
| 23 | `WII_CODE_CC_LEFT` | Classic Left | D-pad | Classic |
| 24 | `WII_CODE_CC_RIGHT` | Classic Right | D-pad | Classic |
| 25 | `WII_CODE_CC_L` | Classic L (analog, clicks at full press) | Hover | Classic |
| 26 | `WII_CODE_CC_R` | Classic R (analog, clicks at full press) | Hover | Classic |
| 27 | `WII_CODE_CC_ZL` | Classic ZL | Digital | Classic |
| 28 | `WII_CODE_CC_ZR` | Classic ZR | Digital | Classic |
| 29 | `WII_CODE_CC_PLUS` | Classic + | Digital | Classic |
| 30 | `WII_CODE_CC_MINUS` | Classic − | Digital | Classic |
| 31 | `WII_CODE_CC_HOME` | Classic Home | Digital | Classic |
| 32 | `WII_CODE_CC_LX_RIGHT` | Classic LS Right | Joystick | Classic |
| 33 | `WII_CODE_CC_LX_LEFT` | Classic LS Left | Joystick | Classic |
| 34 | `WII_CODE_CC_LY_UP` | Classic LS Up | Joystick | Classic |
| 35 | `WII_CODE_CC_LY_DOWN` | Classic LS Down | Joystick | Classic |
| 36 | `WII_CODE_CC_RX_RIGHT` | Classic RS Right | Joystick | Classic |
| 37 | `WII_CODE_CC_RX_LEFT` | Classic RS Left | Joystick | Classic |
| 38 | `WII_CODE_CC_RY_UP` | Classic RS Up | Joystick | Classic |
| 39 | `WII_CODE_CC_RY_DOWN` | Classic RS Down | Joystick | Classic |
| 40 | `WII_CODE_POINTER_RIGHT` | Pointer Right (stick aim) | Joystick | all |
| 41 | `WII_CODE_POINTER_LEFT` | Pointer Left (stick aim) | Joystick | all |
| 42 | `WII_CODE_POINTER_UP` | Pointer Up (stick aim) | Joystick | all |
| 43 | `WII_CODE_POINTER_DOWN` | Pointer Down (stick aim) | Joystick | all |
| 44 | `WII_CODE_POINTER_RECENTER` | Pointer Recenter | Digital | all |
| 45 | `WII_CODE_EXTENSION_TOGGLE` | Extension Attach/Detach | Digital | all |
| 46 | `WII_CODE_REMOTE_FLICK_UP` | Remote Flick Up | Digital | all |
| 47 | `WII_CODE_REMOTE_FLICK_DOWN` | Remote Flick Down | Digital | all |
| 48 | `WII_CODE_REMOTE_FLICK_LEFT` | Remote Flick Left | Digital | all |
| 49 | `WII_CODE_REMOTE_FLICK_RIGHT` | Remote Flick Right | Digital | all |
| 50 | `WII_CODE_NUNCHUK_FLICK_UP` | Nunchuk Flick Up | Digital | Nunchuk |
| 51 | `WII_CODE_NUNCHUK_FLICK_DOWN` | Nunchuk Flick Down | Digital | Nunchuk |
| 52 | `WII_CODE_NUNCHUK_FLICK_LEFT` | Nunchuk Flick Left | Digital | Nunchuk |
| 53 | `WII_CODE_NUNCHUK_FLICK_RIGHT` | Nunchuk Flick Right | Digital | Nunchuk |

"Used by" is a UI suggestion: show those codes in that profile's output picker. The firmware
accepts any code in any Wii profile (for example Remote A in the Classic profile still presses
the Wii Remote's A, which helps in the Wii Menu).

Suggested groups in the picker: **Wii Remote** (0–10), **Nunchuk** (11–16, 50–53),
**Classic Controller** (17–39), **Pointer & Motion** (40–49).

The flicks (46–53) replace the earlier Shake Remote / Shake Nunchuk codes and moved Extension
Attach/Detach from 47 to 45; see `config-app-motion.md`. Games that only check for a shake take
a flick in any direction.

## 6. Mapper commands (`mapper_cmd_t`)

Appended after `MAPPER_CMD_WEBUSB_SINPUT` (13):

| Value | Command | Effect |
| --- | --- | --- |
| 14 | `MAPPER_CMD_DEFAULT_WII_NUNCHUK` | Reset the Wii Nunchuk profile to defaults |
| 15 | `MAPPER_CMD_DEFAULT_WII_CLASSIC` | Reset the Wii Classic profile to defaults |
| 16 | `MAPPER_CMD_DEFAULT_WII_SIDEWAYS` | Reset the Wii Sideways profile to defaults |
| 17 | `MAPPER_CMD_WEBUSB_WII_NUNCHUK` | Start remap preview on the Wii Nunchuk profile |
| 18 | `MAPPER_CMD_WEBUSB_WII_CLASSIC` | Start remap preview on the Wii Classic profile |
| 19 | `MAPPER_CMD_WEBUSB_WII_SIDEWAYS` | Start remap preview on the Wii Sideways profile |

`MAPPER_CMD_DEFAULT_ALL` (1) now also resets the three Wii profiles. Preview end and refresh work as
for every other profile. The Authentic RGB palette uses plain light gray for all Wii outputs (the
real Wii controllers have white/gray buttons).

## 7. Default layouts (Switch-style board)

Derived from the board's Switch defaults; shown here by Switch button name so the app can label
"Reset to defaults" clearly.

| Switch button | Nunchuk profile (Upright) | Classic profile | Sideways profile |
| --- | --- | --- | --- |
| A | Remote A | Classic A | Remote 2 |
| B | Remote B | Classic B | Remote 1 |
| X | Remote 2 | Classic X | Remote A |
| Y | Remote 1 | Classic Y | Remote B |
| D-pad | Remote D-pad | Classic D-pad | Remote D-pad rotated (Up→Right, Down→Left, Left→Up, Right→Down) |
| L | Nunchuk C | Classic ZL | Remote A |
| R | Remote Flick Down | Classic ZR | Remote Flick Down |
| ZL (analog trigger) | Nunchuk Z | Classic L (analog) | Remote B |
| ZR (analog trigger) | Remote B | Classic R (analog) | Remote Flick Down |
| + / − / Home | Remote + / − / Home | Classic + / − / Home | Remote + / − / Home |
| Capture | Extension Attach/Detach | Extension Attach/Detach | Extension Attach/Detach |
| LS click | Nunchuk Flick Down | — | — |
| RS click | Pointer Recenter | Pointer Recenter | Pointer Recenter |
| Left stick | Nunchuk stick | Classic left stick | Remote D-pad rotated (digital) |
| Right stick | Pointer (stick aim) | Classic right stick | Pointer (stick aim) |

## 8. Suggested UI

- Under remapping, add a **Wii** section with three tabs: "Upright", "Sideways" and "Classic",
  each editing its profile above. Hide it when `wii_supported` is 0. Nunchuk outputs are useful in
  the Sideways tab too, since a Nunchuk can be attached there.
- Add Wii to the boot-mode help text: d-pad up = Wii; power tap cycles Upright → Sideways →
  Classic (LED flashes white / yellow / blue); Capture plugs / unplugs the extension; pair by
  pressing SYNC on the Wii. The controller turns itself off a few seconds after the Wii is
  switched off.
- Classic L/R are analog outputs (like GameCube L/R analog): analog inputs pass through and digital
  inputs send their static output value. The Wii sees the L/R click once the value passes ~95%.
- Note in the Wii tabs that the gyro always drives the pointer. The IMU section's sensitivity
  settings scale the pointer speed but also the MotionPlus gyro and the reported tilt, so values
  far from the default can make MotionPlus games and tilt controls feel off.
- Nintendont tip for the Classic tab: rumble with a Classic Controller needs Nintendont's
  "CC Rumble" setting turned on.
