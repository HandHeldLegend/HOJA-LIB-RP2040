#ifndef HAL_NRF52_BLE_FLOSS_H
#define HAL_NRF52_BLE_FLOSS_H

// FLOSS, the Bluetooth host stack, on the nRF52: what it needs from the chip, and its transport to
// the controller in the same chip (hal/nrf52/ble_controller.h). Packets pass by function call where
// another board has a UART or SPI bus to a radio chip.

#include "floss/floss.h"

// The transport for floss_init(). floss_start() starts the controller.
floss_transport_t *nrf52_ble_floss_transport(void);

// What FLOSS saves (a host's keys, its own) is one record in the board's store (hal/record_hal.h).
// A write there stops the CPU for about 130 ms, so it waits: a second after the last change for a
// new or dropped key, and until no host is connected for one that only changed (FLOSS notes which
// host came last at every connection). Each pass of the main loop.
void nrf52_ble_floss_store_task(void);
// Whatever is waiting, now: before the power goes
void nrf52_ble_floss_store_flush(void);

#endif
