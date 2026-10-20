#ifndef UTILITIES_NS2_BT_PROBE_H
#define UTILITIES_NS2_BT_PROBE_H

// Bench builds only (HOJA_NS2_BT_PROBE): Switch mode over Bluetooth Classic claims to be a Pro
// Controller 2 (product 0x2069, its report descriptor) and records what the console sends. Once the
// console has talked for a while (or after a minute) the record goes to the stage header sector and
// the pad drops into update mode, where the bench reads it back.

#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

typedef enum
{
    NS2P_OUT = 1,       // An output report from the console
    NS2P_CONNECTED = 2, // The console connected
    NS2P_LE_EVT = 3,    // An LE event: type, status, handle (2), tx/rx PHY, interval (2), MTU (2)
    NS2P_NOTE = 4,      // Anything else: a code, then its values
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

#endif
