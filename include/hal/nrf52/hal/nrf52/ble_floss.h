#ifndef HAL_NRF52_BLE_FLOSS_H
#define HAL_NRF52_BLE_FLOSS_H

// FLOSS, the Bluetooth host stack, on the nRF52: what it needs from the chip, and its transport to
// the controller in the same chip (hal/nrf52/ble_controller.h). Packets pass by function call where
// another board has a UART or SPI bus to a radio chip.

#include "floss/floss.h"

// The transport for floss_init(). floss_start() starts the controller.
floss_transport_t *nrf52_ble_floss_transport(void);

#endif
