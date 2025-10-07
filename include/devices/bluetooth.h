#ifndef DEVICES_BLUETOOTH_H
#define DEVICES_BLUETOOTH_H

#include <stdint.h>
#include <stdbool.h>

#include "devices/haptics.h"

#include "switch/switch_haptics.h"
#include <stdbool.h>
#include "hoja_shared_types.h"
#include "devices/devices.h"
#include "board_config.h"

typedef enum
{
    BTCB_POWER_CODE,
    BTCB_CONNECTION_STATUS,
    BTCB_SD_RUMBLE,
    BTCB_HD_RUMBLE,
} bluetooth_cb_msg_t;

typedef struct
{
    bluetooth_cb_msg_t type;
    uint8_t data[8];
} bluetooth_cb_msg_s;

typedef void (*bluetooth_cb_t)(bluetooth_cb_msg_s *msg);

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_HAL)
    #include "hal/bluetooth_hal.h"
#elif defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA)
    #include "drivers/bluetooth/esp32_hojabaseband.h"
#endif

void bluetooth_mode_stop();
bool bluetooth_mode_start(gamepad_mode_t mode, bool pairing_mode);

void bluetooth_mode_task(uint64_t timestamp);

// Pass this as our callback handler
void bluetooth_callback_handler(bluetooth_cb_msg_s *msg);

#endif