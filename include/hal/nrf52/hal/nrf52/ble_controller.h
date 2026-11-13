#ifndef HAL_NRF52_BLE_CONTROLLER_H
#define HAL_NRF52_BLE_CONTROLLER_H

// The Bluetooth LE controller on the nRF52 itself: NimBLE's open link layer and radio driver, run
// from interrupts with no operating system (src/hal/nrf52/ble/nimble_port.c). It is spoken to in
// HCI, as a controller in a separate radio chip would be, so the host stack above it needs nothing
// special.
//
// It takes the radio, TIMER0, RTC0, the encryption units, the random number generator and PPI
// channels 20-31 for itself.

#include <stdbool.h>
#include <stdint.h>

// HCI packet types, as on a UART
#define NRF52_BLE_HCI_CMD 0x01
#define NRF52_BLE_HCI_ACL 0x02
#define NRF52_BLE_HCI_EVT 0x04

#define NRF52_BLE_HCI_CMD_MAX_LEN 258u // Header and 255 bytes of parameters

// Starts the controller. It idles until it is given commands.
bool nrf52_ble_controller_start(void);

// The controller's public address, least significant byte first (as on the air): the chip's own,
// from the factory
void nrf52_ble_controller_address(uint8_t out[6]);

// A command or ACL packet for the controller, starting at its HCI header. False when it has no
// buffer free: try again on a later pass.
bool nrf52_ble_hci_send(uint8_t type, const uint8_t *pkt, uint16_t len);

// The next event or ACL packet from the controller, starting at its HCI header: its length, or 0
// when there is none. From the main loop only.
uint16_t nrf52_ble_hci_receive(uint8_t *type, uint8_t *buf, uint16_t max_len);

// Packets from the controller lost because the host did not collect them in time
uint32_t nrf52_ble_hci_dropped(void);

#if defined(HOJA_BENCH_LOG)
#include <stddef.h>

// Bench: one line of the link layer's counters since start (packets, CRC errors, missed anchors)
int nrf52_ble_bench_stats(char *out, size_t max);
#endif

#endif
