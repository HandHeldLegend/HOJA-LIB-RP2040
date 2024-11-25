#ifndef HOJA_BLUETOOTH_H
#define HOJA_BLUETOOTH_H

#include <stdint.h>
#include <stdbool.h>

#include "extensions/haptics.h"

#include "switch/switch_haptics.h"

#include "devices/devices.h"

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

bool bluetooth_init(int device_mode, bool pairing_mode);

void bluetooth_task(uint32_t timestamp);

// Pass this as our callback handler
void bluetooth_callback_handler(bluetooth_cb_msg_s *msg);

#endif