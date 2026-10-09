#ifndef DRIVERS_BLUETOOTH_ESP32_HCI_H
#define DRIVERS_BLUETOOTH_ESP32_HCI_H

// ESP32 running the HOJA HCI bridge firmware: a Bluetooth controller reached over the board's
// I2C bus (see hlink.h). BTstack runs on the RP2040.

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"
#include "hoja_bsp.h"
#include "cores/cores.h"

#if (HOJA_BSP_HAS_I2C==0)
    #error "ESP32 HCI driver requires I2C!"
#endif

#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER==BT_DRIVER_ESP32HCI)
    #if !defined(BLUETOOTH_DRIVER_I2C_INSTANCE)
        #error "BLUETOOTH_DRIVER_I2C_INSTANCE is undefined in board_config.h"
    #endif
    #if !defined(BLUETOOTH_DRIVER_ENABLE_PIN)
        #error "BLUETOOTH_DRIVER_ENABLE_PIN is undefined in board_config.h"
    #endif
    #if !defined(HOJA_USB_MUX_DRIVER)
        #error "HOJA_USB_MUX_DRIVER is required for the ESP32 HCI driver."
    #endif
#endif

// I2C clock for the link
#ifndef ESP32_HCI_I2C_KHZ
#define ESP32_HCI_I2C_KHZ 1000
#endif

// Debug: while Bluetooth runs, USB goes to the ESP32's CH340 for a console that shows the
// RP2040's log and printf and takes a few commands. The RP2040 has no USB while this is on.
#ifndef ESP32_HCI_CONSOLE
#define ESP32_HCI_CONSOLE 0
#endif

// The HCI bridge reports this version or higher, the old baseband reports lower
#define ESP32_HCI_BRIDGE_VERSION_MIN 0xB000

void esp32_hci_log(const char *fmt, ...);

// Used by bluetooth_hal.c. The transport is a const hci_transport_t *.
bool esp32_hci_backend_init(core_params_s *params);
void esp32_hci_backend_task(uint64_t timestamp);
void esp32_hci_backend_stop(void);
bool esp32_hci_backend_update_mode(void);
const void *esp32_hci_transport_instance(void);
// Address the radio starts with next (the stack can't set it on this controller)
void esp32_hci_set_radio_mac(const uint8_t *mac);
void esp32_hci_console_attach(void);

// ESP32 firmware version, read once (0 if it does not answer)
uint16_t esp32_hci_firmware_version(void);

// Old baseband driver, used until the ESP32 is updated
bool esp32_legacy_bt_init(core_params_s *params);
void esp32_legacy_bt_task(uint64_t timestamp);
void esp32_legacy_bt_stop(void);

// Battery voltage from the ESP32 (0 until measured)
uint16_t esp32_hci_battery_mv(void);

#endif
