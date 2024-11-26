#ifndef DRIVERS_BLUETOOTH_ESP32HOJA_H
#define DRIVERS_BLUETOOTH_ESP32HOJA_H

// Requires I2C setup

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"
#include "hoja_bsp.h"

#include "devices/bluetooth.h"

#include "input/button.h"
#include "input/imu.h"
#include "input/analog.h"

#if (HOJA_BSP_HAS_I2C==0)
    #error "ESP32 Hoja Baseband driver requires I2C!" 
#endif

#if defined(BLUETOOTH_DRIVER_ESP32HOJA) && (BLUETOOTH_DRIVER_ESP32HOJA>0)

#if !defined(BLUETOOTH_DRIVER_I2C_INSTANCE)
    #error "BLUETOOTH_DRIVER_I2C_INSTANCE is undefined in board_config.h"
#endif

#if !defined(BLUETOOTH_DRIVER_ENABLE_PIN)
    #error "BLUETOOTH_DRIVER_ENABLE_PIN is undefined in board_config.h"
#endif

// REQUIRE USB_MUX driver
#if !defined(HOJA_USB_MUX_DRIVER)
    #error "HOJA_USB_MUX_DRIVER is required for ESP32 Hoja Baseband driver."
#else

#if HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA
    #define HOJA_BLUETOOTH_INIT(device_mode, pairing_mode, evt_cb) esp32hoja_init(device_mode, pairing_mode, evt_cb)
    #define HOJA_BLUETOOTH_TASK(timestamp) esp32hoja_task(timestamp)
    #define HOJA_BLUETOOTH_GETINFO() esp32hoja_get_info();
#endif

// Define types

bool esp32hoja_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb);

void esp32hoja_stop();

void esp32hoja_task(uint32_t timestamp);

int esp32hoja_hwtest();

uint32_t esp32hoja_get_info();

#endif
#endif 

#endif