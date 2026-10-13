# HOJA code style

How HOJA code is written: the firmware library, the dongle library, the board firmware, and the
tools around them (the PC app, bench scripts). It comes from the existing code. Where that code
does a thing two ways, this guide picks one; older code moves to it when it is next touched, not
in sweeping reformat commits.

The aim is code that reads easily a year later: clear names, small functions, comments that say
why.

## 1. Layout

- 4-space indentation, no tabs. Lines up to 110 columns.
- LF line endings in the repository (Git may check out CRLF on Windows).
- Braces on their own line (Allman), for functions, control blocks, structs and unions:

  ```c
  static void _core_slippi_set_connected(bool connected)
  {
      if (connected == _slippi_connected)
          return;

      _slippi_connected = connected;
  }
  ```

- A space after `if`, `for`, `while` and `switch`, none after a function name: `if (len < 2)`,
  `rgb_set_pulsing(color)`.
- A single-statement body may go without braces, but always on its own line. Once one branch of an
  `if`/`else` needs braces, they all get them.
- `case` labels indented one level inside `switch`, their statements one level more:

  ```c
  switch (report_id)
  {
      case 0x11:
          _core_slippi_rumble(data[1] & 0x1);
          break;

      default:
          break;
  }
  ```

- One blank line between functions and between logical steps inside one; no runs of blank lines.
- Columns may be aligned where it makes a table easier to read: `#define` values, struct fields
  with their comments, a block of related assignments.

## 2. Names

| Kind | Rule | Example |
|---|---|---|
| Public functions | `module_verb_object` | `rgb_set_pulsing`, `core_gamecube_init` |
| File-local functions | `static`, `_module_verb_object` | `_rgb_cap_brightness_wireless` |
| File-local variables | `static`, `_name` | `_slippi_connected` |
| Structs | `name_s` | `rgb_group_cfg_s`, `boot_info_s` |
| Enums, other typedefs | `name_t` | `gamepad_transport_t`, `callback_t` |
| Macros and constants | `MODULE_NAME`, unit last | `NESBUS_DISCONNECT_US`, `BT_HAL_APP_PAIR_MS` |
| Build options | `HOJA_*` | `HOJA_RGB_DRIVER`, `HOJA_TRANSPORT_WLAN_DRIVER` |
| Header guards | `DIR_FILE_H` | `DEVICES_RGB_H` |

- Everything file-local is `static`. A leading `_` means "file-local", so a non-static `_name` is
  a bug to fix.
- snake_case throughout. The camelCase config types (`gamepadConfig_s` and friends) are legacy;
  don't add more.
- Units go in the name: `_MS`, `_US`, `_HZ`, `_LEN`, `_MV`, `_MA`. Time in code is microseconds
  unless the name says otherwise; tasks take `uint64_t timestamp` in µs.
- Booleans read as a statement: `_connected`, `_pairing_mode`, `link_called`.
- Names say what a thing is for, not how it's built: `_app_pair_pc`, not `_tmp_addr`.

## 3. Files

- Own header first, then C and SDK headers, then project headers, grouped by area:

  ```c
  #include "devices/rgb.h"

  #include <math.h>
  #include <string.h>

  #include "utilities/settings.h"
  #include "hoja.h"
  ```

- Order inside a `.c` file: includes, `#define`s, types, static state, static helpers, public
  functions. Forward-declare a static only when the order can't avoid it.
- A driver or HAL file that only builds for some boards is wrapped whole in its option:
  `#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)`.
- Long files are split into sections with one marker style:

  ```c
  // --- Connection lifecycle ---
  ```

## 4. Code

- Fixed-width integer types (`uint8_t`, `int16_t`, `uint32_t`), `bool` from `<stdbool.h>`.
- No magic numbers: name them with a `#define` that carries the unit, and comment the why
  (`#define BT_HAL_APP_HOST_WAIT_MS 4000u // HID up but no app: the dongle link takes over`).
- Designated initializers for structs; compound literals for events:

  ```c
  transport_evt_cb((tp_evt_s){
      .evt = TP_EVT_PLAYERLED,
      .evt_playernumber = {.player_number = connected ? 1u : 0u},
  });
  ```

- Report layouts may be bitfield unions over a whole value (`struct { ... }; uint32_t value;`),
  as the cores do. Byte-order-sensitive wire formats in Bluetooth and protocol code are built with
  explicit byte writes instead.
- Board settings come from `hoja_config_get()`; check it for `NULL`.
- Unused parameters: `(void)arg;`.
- Early returns over nesting. Keep functions short enough to read in one screen; split one
  that does several things.
- No blocking waits in tasks or stack callbacks: set a flag or a deadline and let the task act
  on it.
- No heap.

## 5. Comments

- `//` line comments. `/* */` only for spec citations and the rare block of notes.
- Say why, the constraint, or the unit, not what the next line plainly does.
- Sentence case. A one-line comment has no full stop; a comment of several sentences uses them.
- Describe how things are, not how they came to be. History belongs in commit messages, not in
  a comment like "fixed the old way".
- Measurements: state them as measured, with the number and the hardware ("measured on a GCU-2:
  paired in about a second"). Never write a cause as fact until it's shown.
- Credit borrowed ideas with a link, as `core_gamecube.h` does for PhobGCC.
- Public functions get a short comment above their declaration in the header: what it does, and
  anything the caller must know (context, units, ownership).

## 6. Bench and diagnostic code

- Instrumentation for an investigation goes behind a `HOJA_*` build flag (set in the board's
  `board.cmake` for a bench build, never committed there).
- When the investigation is done, the instrumentation is deleted; git history keeps it. The
  findings go in the commit message or the docs.

## 7. Commits

- Subject: `<area>: <imperative summary>`. At most 72 characters, no full stop.
  - Areas: `bt`, `link`, `wlan`, `usb`, `core/<name>`, `input`, `rgb`, `haptics`, `battery`,
    `hal`, `boot`, `settings`, `config`, `build`, `docs`, `tools`.
  - In a board firmware repo: `deps` for library pointer updates, `builds` for release images.
- Body: why, then what, wrapped at 72 columns. Include measurements that justify the change.
- One logical change per commit.

```
link: pair from a Bluetooth mode without restarting the gamepad

The radio alone restarts as Link mode's and pages the PC that asked;
the gamepad restarts in Link mode once that PC has set up the HID
device. Measured on a GCU-2: 6.5 s from the request to Link mode,
where restarting into dongle mode first stalled until ~20 s after boot.
```

## 8. Other languages

The same rules, in each language's own idiom.

**C# (the PC app)**
- PascalCase for types, methods, properties and constants; `_camelCase` for private fields;
  camelCase for locals. Units in names, as in C (`PagedSeconds`, `SlowPairMs`).
- One type per file, opening with a comment saying what the file does.
- Allman braces, 4 spaces, nullable reference types on.
- P/Invoke declarations at the bottom of their class.

**Python (bench scripts)**
- PEP 8, 4 spaces, a module docstring with usage examples.
- Scripts run with `python -I`.
