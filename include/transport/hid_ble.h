#ifndef TRANSPORT_HID_BLE_H
#define TRANSPORT_HID_BLE_H

// A mode's HID gamepad over Bluetooth LE (HID over GATT, FLOSS's floss_hogp), for a radio that has
// Bluetooth LE alone (BT_DRIVER_NRF52). The mode's own report descriptor, name and USB ids go out as
// they are, so a host sees the gamepad it sees on a cable. SInput is the mode that has it.
//
// A new host pairs when the gamepad was started to pair, or has no host yet; a host it knows takes
// it back whenever it is on. FLOSS keeps the keys (floss_hal_store_*).
//
// It goes by an address of its own, made from the radio's. A host that has paired with the gamepad
// connects to that address whenever it sees it, whatever is advertised from it: on the radio's own
// address a computer took the link the Switch 2 mode was offering its console.

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"

// After floss_init(), before floss_start(). radio_mac is the radio's public address, most
// significant byte first. pairing: any host may pair. False for a mode without reports here. Again
// after hid_ble_stop() to carry on as it was started.
bool hid_ble_start(core_params_s *params, const uint8_t radio_mac[6], bool pairing);

// Leaves the host and goes quiet
void hid_ble_stop(void);

// Each pass of the main loop, after floss_poll()
void hid_ble_task(void);

// A host is reading the gamepad
bool hid_ble_linked(void);

// A host is saved (started or not), and forgetting every one
bool hid_ble_paired(void);
void hid_ble_forget(void);

#endif
