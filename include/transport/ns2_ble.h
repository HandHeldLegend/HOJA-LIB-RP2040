#ifndef TRANSPORT_NS2_BLE_H
#define TRANSPORT_NS2_BLE_H

// A Pro Controller 2 for a Switch 2, over Bluetooth LE: NS2-LIB-HID's GATT table and protocol on
// FLOSS. The console wants the 2M PHY, so this is for radios that have it (the nRF52; not the RM2).
//
// For now it runs beside whatever mode the gamepad started in, with the buttons on fixed positions
// (ns2_ble_input): it is not yet a mode of its own with a profile.

#include <stdbool.h>
#include <stdint.h>

#include "ns2_lib.h"

// After floss_init(), before or after floss_start(). dev_mac is the radio's public address, most
// significant byte first: the console knows the controller by it.
bool ns2_ble_start(const uint8_t dev_mac[6]);

// Each pass of the main loop, after floss_poll()
void ns2_ble_task(void);

// A console is connected
bool ns2_ble_linked(void);

// Started: the NS2 library's input hook (core_ns2gc.c) hands over to ns2_ble_input
bool ns2_ble_running(void);
void ns2_ble_input(ns2_input_s *out);

#endif
