#ifndef HAL_RP2040_BLE_CONFIG_H
#define HAL_RP2040_BLE_CONFIG_H

// The config app over Bluetooth LE while a wireless mode runs (FLOSS boards). Plus and Minus
// pressed together advertise a config service for 10 s. A browser proves it may connect with a
// token from an earlier session, or by a one-time code of six presses entered on the gamepad.

#include <stdint.h>
#include <stdbool.h>

// Wireless links run no faster than this while BLE config is active (the beacon or a connected app),
// leaving it air time. A host asked to poll every slot pair is asked for this latency meanwhile: the
// ESP32's controller gets no gap for LE otherwise.
#define BLE_CONFIG_REPORT_INTERVAL_US 8000u
#define BLE_CONFIG_POLL_LATENCY_US    7500u

// After floss_init, before floss_start: the GATT service (the table is fixed once the radio runs)
// and the LE listener. name: shown in the browser's device list. slow_beacon: a link that
// fills the air (the dongle at 1.25 ms) is running, so advertise gently.
void ble_config_setup(const char *name, bool slow_beacon);

// From the Bluetooth task: the Plus + Minus press, the beacon window, code entry, app traffic
void ble_config_task(uint64_t now_us);

// An app is connected over LE
bool ble_config_connected(void);

// The beacon runs or an app is connected: links leave LE air time
bool ble_config_active(void);

// Open the beacon as Plus + Minus would (the radio just came up for it, in a wired mode)
void ble_config_open_beacon(void);

#endif
