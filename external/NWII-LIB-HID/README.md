# NWII-LIB-HID

Portable C11 library that makes a microcontroller look like a **Wii Remote (RVL-CNT-01)** to a Wii,
with an optional **Nunchuk**, **Classic Controller** or **Classic Controller Pro** plugged in.
Bring your own Bluetooth Classic stack (BTstack on a Pico W, for example); the library owns the
Wii Remote protocol.

What the library handles:

- every data reporting mode (0x30–0x37, 0x3D, interleaved 0x3E/0x3F)
- status, acknowledgement and memory-read replies, including multi-packet EEPROM reads
- extension identification, calibration, hotplug and the extension encryption Wii games enable
- IR camera reports, plus a virtual sensor bar (`nwii_ir_set_pointer`) so a gyro or stick can
  drive the pointer
- Bluetooth identity blobs: device name, class of device, HID descriptor, the SDP record the Wii
  checks, and the pairing PIN

Not emulated: Wii MotionPlus, the speaker (audio is accepted and discarded), and the Balance Board.

## Free for everyone

NWII-LIB-HID is released into the public domain under **[The Unlicense](LICENSE)**. Use it in
anything — hobby, commercial, closed source — with no attribution required.

**AI disclosure:** this library was written with the help of **Claude Opus** (Anthropic), working
from public Wii Remote protocol documentation and existing open emulator projects.

## Documentation

- **[docs/implementation-guide.md](docs/implementation-guide.md)** — integration walkthrough
  (CMake, Bluetooth setup, pairing, the tunnel / generate loop, hooks)

## Quick start

```cmake
add_subdirectory(path/to/NWII-LIB-HID)
target_link_libraries(your_firmware PRIVATE nwii_lib_hid)
```

```c
#include "nwii_lib.h"

nwii_device_config_s cfg = { .extension = NWII_EXTENSION_NUNCHUK };
nwii_api_init(&cfg);

// Host output report (without the 0xA2 header) -> library
nwii_api_output_tunnel(report, len);

// On your report timer (~100 Hz)
uint8_t data[NWII_INPUT_REPORT_MAX];
uint8_t len;
if (nwii_api_generate_inputreport(data, &len))
{
    // send 0xA1 + data[0..len) on the HID interrupt channel
}

// Firmware supplies input
void nwii_api_hook_get_input(nwii_input_s *out)
{
    out->remote.a = button_a_pressed();
    out->nunchuk.stick_x = stick_x_12bit();
    nwii_ir_set_pointer(out->ir, cursor_x, cursor_y);
}
```

## Trademarks

This library is not affiliated with, authorized or endorsed by Nintendo Co., Ltd. Nintendo and Wii
are trademarks of their respective owners.
