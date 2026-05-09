# NS-LIB-HID implementation guide

This guide covers practical firmware integration for `NS-LIB-HID` now that core protocol, haptics, SPI emulation, and IMU paths are in place.

## 1) Integration model

`NS-LIB-HID` runs as a small stateful protocol engine:

- You initialize once with `ns_lib_init(...)`.
- You pass incoming host OUT packets into `ns_api_output_tunnel(...)`.
- The library queues those packets in a FIFO (depth 3).
- Your main loop periodically calls `ns_api_generate_inputreport(...)`.
- Each generate call:
  - processes at most one queued host command packet (if present), or
  - emits normal input report `0x30` if no command is pending.

This keeps host command handling deterministic and synchronized with your report cadence.

## 2) Build and include

From a parent CMake project:

```cmake
add_subdirectory(path/to/NS-LIB-HID)
target_link_libraries(your_firmware PRIVATE ns_lib_hid)
```

Include public API:

```c
#include "ns_lib.h"
```

## 3) One-time initialization

Create and fill `ns_device_config_s`, then call `ns_lib_init(&cfg)`.

Required fields:

- `type` (not `NS_DEVTYPE_UNDEFINED`)
- `transport` (not `NS_TRANSPORT_UNDEFINED`)
- optional but recommended: `device_mac`, `host_mac`, `colors`
- `gyro_full_scale_dps` (if <= 0, defaults to 2000 dps)

Example:

```c
ns_device_config_s cfg = {0};
cfg.type = NS_DEVTYPE_PROCON;
cfg.transport = NS_TRANSPORT_USB;
cfg.gyro_full_scale_dps = 2000.0f;
// fill cfg.device_mac / cfg.host_mac / cfg.colors as needed

ns_config_status_t st = ns_lib_init(&cfg);
if (st != NS_CONFIG_OK) {
    // handle configuration error
}
```

Notes:

- `ns_lib_init()` runs config validation, computes `gyro_rad_per_lsb`, initializes analog calibration defaults, and initializes haptics state.
- If you need to reconfigure at runtime, use `ns_device_config_set(...)`.

## 4) Host OUT path (enqueue API)

Feed every received host OUT packet to:

```c
ns_api_output_tunnel(out_data, out_len);
```

Behavior:

- Packet is enqueued into FIFO (max 3 packets).
- Processing is deferred to generate-time.
- FIFO full -> newest packet is dropped (enqueue returns false internally).

Recommended firmware policy:

- Call `ns_api_output_tunnel(...)` as soon as OUT data arrives.
- Keep generate cadence high enough to drain queue under bursty host traffic.

## 5) Generate path (main loop)

Call this on your normal report tick:

```c
uint8_t tx[64] = {0};
ns_api_generate_inputreport(tx, sizeof(tx));
```

Output:

- `tx[0]` = report ID
- `tx[1..]` = payload

If a command/info packet is queued, generated output is a command reply (`0x21` or `0x81`) with command effects applied in FIFO order.
If queue is empty, generated output is normal report `0x30`.

## 6) Required callback overrides

The library provides weak defaults; real products should override these in platform code:

- `ns_get_inputdata_cb(ns_inputdata_s *out)`
  - Fill buttons and packed sticks.
- `ns_get_powerstatus_cb(ns_powerstatus_s *out)`
  - Set battery/charging/connection byte.
- `ns_get_imu_standard_cb(ns_gyrodata_s *out)` (if IMU standard mode enabled)
- `ns_get_imu_quaternion_cb(ns_quaternion_s *out)` (if IMU mode-2 enabled)

Useful host-effect callbacks:

- `ns_set_led_cb(int player_leds)`
- `ns_set_imumode_cb(ns_imu_mode_t mode)`
- `ns_set_haptic_indices_cb(...)`
- `ns_set_power_cb(uint8_t shutdown)`
- `ns_set_usbpair_cb(...)`

Platform utility hooks:

- `ns_get_time_ms(uint64_t *ms)`
- `ns_get_random_u8(void)`

## 7) Stick packing and calibration

For 12-bit Switch nibble layout use:

- `ns_analog_pack_xy12(x, y, out3)`

Byte mapping:

- `out3[0] = x[7:0]`
- `out3[1] = x[11:8] | y[3:0] << 4`
- `out3[2] = y[11:4]`

Calibration blob for SPI stick pages is initialized by `ns_analog_calibration_init()` during `ns_lib_init()`.

## 8) IMU and gyro scaling

Set `cfg.gyro_full_scale_dps` for your IMU range (for example 2000 for +-2000 dps).
`NS-LIB-HID` derives `gyro_rad_per_lsb` once during config set:

- `rad/s per LSB = (full_scale_dps / 32768) * (pi/180)`

Quaternion integration helpers consume this precomputed scalar.

## 9) Minimal loop template

```c
void on_host_out_packet(const uint8_t *data, uint16_t len)
{
    ns_api_output_tunnel((uint8_t *)data, len);
}

void protocol_tick(void)
{
    uint8_t report[64] = {0};
    ns_api_generate_inputreport(report, sizeof(report));
    transport_send(report, sizeof(report)); // USB/BT send path
}
```

## 10) Common pitfalls

- Calling generate too slowly while host sends multiple commands quickly (FIFO can overflow).
- Not overriding weak callbacks, causing all-zero input and placeholder state.
- Forgetting to set `type`/`transport` before `ns_lib_init()`.
- Mixing API names from older trees; standardize on `NS-LIB-HID` symbols in your firmware.

## 11) Bring-up checklist

- Build links with `ns_lib_hid`.
- `ns_lib_init()` returns `NS_CONFIG_OK`.
- OUT packets are passed into `ns_api_output_tunnel()`.
- Generate loop calls `ns_api_generate_inputreport()` at steady interval.
- Verified command bursts (up to 3) are handled in-order.
- Verified idle behavior emits `0x30` input reports.

