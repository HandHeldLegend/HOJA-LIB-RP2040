#ifndef TRANSPORT_NS2_BLE_H
#define TRANSPORT_NS2_BLE_H

// A Pro Controller 2 for a Switch 2, over Bluetooth LE: NS2-LIB-HID's GATT table and protocol on
// FLOSS. The console wants the 2M PHY, so this is for radios that have it (the nRF52; not the RM2).
//
// A build with it (HOJA_NS2_BLE) has it as Switch mode over Bluetooth: core_switch.c hands that
// transport over, and the mode's profile, colour and saved host are the Switch's. The console pairs
// by its own commands over the link; the key it agrees is saved where the Switch's key goes.

#include <stdbool.h>
#include <stdint.h>

#include "ns2_lib.h"

// After floss_init(), before or after floss_start(). dev_mac is the radio's public address, most
// significant byte first: the console knows the controller by it. pairing: started as a controller
// no console knows, so any console finds it, though one is saved (without one saved it always is).
// Again after ns2_ble_stop() to carry on as it was started.
bool ns2_ble_start(const uint8_t dev_mac[6], bool pairing);

// Leaves the console and goes quiet
void ns2_ble_stop(void);

// Each pass of the main loop, after floss_poll()
void ns2_ble_task(void);

// A console is connected
bool ns2_ble_linked(void);

// Started: the NS2 library's input hook (core_ns2gc.c) hands over to ns2_ble_input
bool ns2_ble_running(void);
void ns2_ble_input(ns2_input_s *out);

// A console is saved (started or not), and forgetting it
bool ns2_ble_paired(void);
void ns2_ble_forget(void);

#endif
