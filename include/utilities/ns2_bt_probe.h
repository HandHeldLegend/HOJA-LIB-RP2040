#ifndef UTILITIES_NS2_BT_PROBE_H
#define UTILITIES_NS2_BT_PROBE_H

// Bench builds only (HOJA_NS2_BT_PROBE): Switch mode over Bluetooth Classic claims to be a Pro
// Controller 2 (product 0x2069, its report descriptor) and records what the console sends. Once the
// console has talked for a while (or after a minute) the record goes to the stage header sector and
// the pad drops into update mode, where the bench reads it back.
//
// HOJA_NS2_BLE_PROBE: Switch pairing advertises over LE instead. The NSO GameCube controller on USB
// pairs like a retail controller and keeps the key (in the stage header sector's last bytes); with
// that pairing stored, the LE advert reconnects to that console as the paired controller and serves
// NS2-LIB-HID's attribute table, to see whether a console that already holds the key accepts a
// radio without the 2M PHY.

#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

typedef enum
{
    NS2P_OUT = 1,       // An output report from the console
    NS2P_CONNECTED = 2, // The console connected
    NS2P_LE_EVT = 3,    // An LE event: type, status, handle (2), tx/rx PHY, interval (2), MTU (2), level
    NS2P_NOTE = 4,      // Anything else: a code, then its values
    NS2P_ATT_WRITE = 5, // The console wrote: handle (2), then the bytes
    NS2P_ATT_READ = 6,  // The console read: handle (2), offset (2)
} ns2_bt_probe_kind_t;

#if (defined(HOJA_NS2_BT_PROBE) && HOJA_NS2_BT_PROBE) || (defined(HOJA_NS2_BLE_PROBE) && HOJA_NS2_BLE_PROBE)
#define HOJA_NS2_PROBE_LOG 1
void ns2_bt_probe_record(ns2_bt_probe_kind_t kind, const uint8_t *data, uint16_t len);
void ns2_bt_probe_task(uint64_t now_us);
// Save the record now and drop into update mode
void ns2_bt_probe_flush(void);
#endif

#if defined(HOJA_NS2_BT_PROBE) && HOJA_NS2_BT_PROBE
// Switch mode on Bluetooth: take on the Pro Controller 2's identity
void ns2_bt_probe_identity(core_hid_device_t *device);
#endif

#if defined(HOJA_NS2_BLE_PROBE) && HOJA_NS2_BLE_PROBE && defined(HOJA_NS2)
#include "floss/floss_le.h"

// The console and key from a USB pairing (key MSB first, as ns2_pairing_s has it)
void ns2_bt_probe_pairing_save(const uint8_t host_mac[6], const uint8_t ltk[16]);
bool ns2_bt_probe_pairing_load(uint8_t host_mac[6], uint8_t ltk[16]);

// With a stored pairing: become that paired controller on LE (NS2-LIB-HID's attribute table from
// handle 1, the key for the console's encryption) and fill adv with the reconnect advert. Returns
// the advert's length, 0 without a pairing.
uint8_t ns2_bt_probe_reconnect_setup(const uint8_t dev_mac[6], uint8_t adv[31]);
void ns2_bt_probe_le_event(const floss_le_evt_t *evt);
// Each Bluetooth task pass: command replies and input reports out
void ns2_bt_probe_ble_task(void);

#if defined(HOJA_NS2_CYW_SCAN) && HOJA_NS2_CYW_SCAN
// Bench: look for the radio's LE feature mask in its memory (Broadcom Read_RAM), set the 2M PHY
// bit wherever it sits in RAM, read the mask back, record it all, then update mode
void ns2_bt_probe_cyw_scan_start(void);
#endif
#endif

#endif
