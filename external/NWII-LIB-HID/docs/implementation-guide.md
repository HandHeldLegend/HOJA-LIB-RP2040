# NWII-LIB-HID implementation guide

This guide covers wiring NWII-LIB-HID into a firmware with a Bluetooth Classic stack. Examples use
BTstack (as shipped with the Raspberry Pi Pico SDK), but any stack that exposes HCI events and the
two HID L2CAP channels works.

## 1) Integration model

The library is a small stateful protocol engine:

- initialize it once with `nwii_api_init(...)`
- call `nwii_api_connection_reset()` whenever the Wii opens a new HID connection
- feed host output reports into `nwii_api_output_tunnel(...)`
- call `nwii_api_generate_inputreport(...)` at your report cadence (a real remote sends ~100 Hz)
- override the weak `nwii_api_hook_*` functions to provide input, rumble and LEDs

`nwii_api_output_tunnel` only queues the report (and fires the rumble / LED hooks immediately).
All other processing happens inside `nwii_api_generate_inputreport`, so protocol state is owned by
a single context. The two calls may run in different contexts (thread vs. Bluetooth IRQ, for
example) as long as each has only one caller.

## 2) Bluetooth identity

Configure the local controller before powering it on:

| Setting | Value |
| --- | --- |
| Device name | `nwii_hid_get_device_name()` → `"Nintendo RVL-CNT-01"` |
| Class of device | `NWII_HID_CLASS_OF_DEVICE` (0x002504) |
| Inquiry access codes | **limited (`NWII_HID_INQUIRY_ACCESS_CODE`, 0x9E8B00) and general** — the Wii's SYNC search only uses the limited code, so a device answering just the general one is never found |
| Secure Simple Pairing | **disabled** — the Wii only does legacy PIN pairing |
| L2CAP security level | none (`LEVEL_0`) — the Wii authenticates when it wants to; requesting it yourself breaks temporary (1+2 style) connections |
| Bondable | yes, so the link key from SYNC pairing is stored |
| SDP | register `nwii_hid_get_sdp_record()` verbatim, before allocating other record handles |
| HID descriptor | `nwii_hid_get_report_descriptor()` |
| PnP / Device ID | optional; VID `NWII_HID_VID`, PID `NWII_HID_PID` |

BTstack example:

```c
gap_ssp_set_enable(0);
gap_set_security_level(LEVEL_0);
gap_set_bondable_mode(1);
gap_set_class_of_device(NWII_HID_CLASS_OF_DEVICE);
gap_set_local_name(nwii_hid_get_device_name());

// Once HCI is working (BTSTACK_EVENT_STATE), and the controller can take a command:
hci_send_cmd(&hci_write_current_iac_lap_two_iacs, 2,
             NWII_HID_INQUIRY_ACCESS_CODE, GAP_IAC_GENERAL_INQUIRY);

const uint8_t *record;
nwii_hid_get_sdp_record(&record, NULL);
sdp_register_service(record);

const uint8_t *desc;
uint16_t desc_len;
nwii_hid_get_report_descriptor(&desc, &desc_len);
hid_device_init(0, desc_len, desc);
```

## 3) Pairing and reconnecting

**First pairing (SYNC):** make the device discoverable and connectable, then press the red SYNC
button on the Wii. The Wii finds the device by name, connects, and starts legacy pairing. Answer
the PIN request with the Wii's address reversed:

```c
case HCI_EVENT_PIN_CODE_REQUEST: {
    bd_addr_t addr;
    uint8_t pin[NWII_HID_PIN_LEN];
    hci_event_pin_code_request_get_bd_addr(packet, addr);
    nwii_hid_make_pin(addr, pin);
    gap_pin_code_response_binary(addr, pin, NWII_HID_PIN_LEN);
    break;
}
```

The Wii then opens the HID control (PSM 0x11) and interrupt (PSM 0x13) channels. Save the Wii's
address for reconnecting.

**Reconnecting:** a paired remote connects to the Wii, not the other way around. Open the control
and interrupt channels to the saved address (`hid_device_connect` in BTstack). The Wii accepts
remotes it knows; if it authenticates, the stored link key answers it. Retry periodically if the
Wii is not up yet.

**Title launch and quit:** starting or quitting a game reloads the Wii's system software. The
Wii stops servicing the link while it reloads, and its restarted stack does not know the old
L2CAP channels. Captured behavior on a real Wii:

- On launch the link goes silent; sometimes the new stack then addresses stale channel IDs
  (BTstack answers with an L2CAP Command Reject, "invalid CID").
- On quit the Wii sends L2CAP Disconnect Requests for both HID channels but keeps the ACL link.
- The Wii is the master, so its 20 s supervision timeout decides when a silent link dies.

A real remote is back within a couple of seconds, so a firmware should: drop the ACL link when
reports stop being accepted for ~1.5 s, when it rejects an invalid CID, or when the HID channels
close; then page the saved Wii again with a short page timeout (~5 s), retrying every second, and
call `nwii_api_connection_reset()` when the HID connection reopens. Only power off when the
disconnect reason is "remote device terminated connection due to power off" (0x15), which is
what a Wii sends when it shuts down.

The Wii also accepts temporary connections from any discoverable remote while it is running, which
is useful for testing before a permanent SYNC.

## 4) Report flow

Output reports arrive on the interrupt channel as `0xA2 <id> <payload>`. Strip the `0xA2` header
and pass `<id> <payload>` to `nwii_api_output_tunnel`. Some stacks deliver them through a HID
SET_REPORT path instead; pass those the same way.

Input reports go out on the interrupt channel as `0xA1` followed by the bytes from
`nwii_api_generate_inputreport`. The generator always produces a report: queued replies
(acknowledgements, status, memory reads) first, then the data report for the host's chosen mode.

## 5) Hooks

| Hook | Called from | Purpose |
| --- | --- | --- |
| `nwii_api_hook_get_input` | generator | Fill buttons, accelerometer, IR points and extension state |
| `nwii_api_hook_set_rumble` | tunnel | Motor on/off whenever it changes |
| `nwii_api_hook_set_leds` | tunnel | Player LED bitmask (bit 0 = LED 1) |
| `nwii_api_hook_get_power` | generator | Battery level for status reports |

`nwii_input_s` arrives pre-filled with a neutral state (sticks centred, accelerometer reading
+1 g on Z, IR points invisible), so only override what your hardware has.

### Accelerometer frame

Values are milli-g in the remote's frame: +Z out of the button face, +Y toward the IR camera,
+X toward the remote's left side (verified axis by axis against a real remote; note the frame is
left-handed). A remote lying face-up reads (0, 0, +1000).

### Pointer

Real remotes see the two sensor bar LEDs through an IR camera. `nwii_ir_set_pointer(out->ir, x, y)`
synthesizes those points from a cursor position in -1..+1 (left..right, bottom..top), so any
source works: integrate a gyro, move with a stick, or map a touch surface. Call
`nwii_ir_clear(out->ir)` to report the remote pointing away from the screen.
`nwii_ir_set_pointer_rotated()` also tilts the sensor bar by the remote's roll, which is how the
Wii rotates its cursor; take the roll from the same accelerometer data you report.

### Motion aiming (`nwii_lib_aim.h`)

For a gamepad with a gyro and accelerometer, the aim helper does the whole job:

```c
static nwii_aim_s aim;
nwii_aim_init(&aim, NULL);              // once; NULL = tested defaults

// every input report:
nwii_aim_update(&aim, gyro_dps, accel_g, dt_s);
if (recenter_pressed) nwii_aim_recenter(&aim);
nwii_aim_nudge(&aim, stick_dx, stick_dy); // optional stick aim
nwii_aim_to_ir(&aim, out->ir, true);      // position + cursor roll
nwii_aim_level_accel(&aim, accel, accel); // remote tilt relative to the recentre pose
```

It fuses the two sensors into a gravity estimate and aims in "player space" (after the
GyroWiki): up/down is the controller's own pitch, left/right is rotation about the real vertical
taken from its yaw and roll axes. Aim therefore keeps working however the gamepad is held, flat,
rolled, or pointed straight up or down, which matters for players who cannot hold a controller
the usual way. The gyro offset is learned whenever the controller rests still. Sensors must be
in the controller frame documented in the header (+X left, +Y toward the player, +Z up out of the
face, right-handed for both accelerometer and gyro; check the gyro's roll axis sign, a flipped
one shows up as cursor tilt that lags behind the controller).

Recentring also makes the current pose "level": pass the remote accelerometer through
`nwii_aim_level_accel()` so a player aiming from below or above still reports a remote held
level at the screen.

The virtual sensor bar sits `NWII_IR_POINTER_OFFSET_Y` above the aim point, as the Wii expects,
so a recentred cursor lands in the middle of the screen.

## 6) Extensions

Pick the starting extension in `nwii_device_config_s` and switch at runtime with
`nwii_api_set_extension(...)`. The library emulates an unplug followed by a plug-in, and the Wii
re-identifies the new extension automatically. Extension data is encrypted whenever the host
enables encryption, so games that use it work without extra code.
