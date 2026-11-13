#ifndef HAL_NRF52_BLUETOOTH_H
#define HAL_NRF52_BLUETOOTH_H

// The Bluetooth transport on the nRF52 (BT_DRIVER_NRF52): FLOSS over the controller in the same
// chip. The radio has Bluetooth LE alone, and the transport carries two modes over it: Switch, as a
// Switch 2's Pro Controller 2 (transport/ns2_ble.h, a build with HOJA_NS2_BLE), and SInput, as a HID
// gamepad (transport/hid_ble.h). One to a start of the firmware.
//
// transport_bt_init() and transport_bt_task() use these two. They are here for a bench build that
// runs the Switch 2 link beside a USB mode, where the log can be read.

#include <stdbool.h>

// FLOSS and the controller, once, then the Switch 2 link. pairing: found by any console. False in a
// build without that mode.
bool nrf52_bluetooth_start(bool pairing);

// Each pass of the main loop
void nrf52_bluetooth_task(void);

#endif
