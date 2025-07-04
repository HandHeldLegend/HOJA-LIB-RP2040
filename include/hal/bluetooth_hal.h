#ifndef BLUETOOTH_HAL_H
#define BLUETOOTH_HAL_H

#include "board_config.h"
#include "devices/bluetooth.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_HAL)

#define HOJA_BLUETOOTH_INIT(device_mode, pairing_mode, evt_cb) bluetooth_hal_init(device_mode, pairing_mode, evt_cb)
#define HOJA_BLUETOOTH_TASK(timestamp) bluetooth_hal_task(timestamp)

bool bluetooth_hal_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb);
void bluetooth_hal_task(uint64_t timestamp);

#endif
#endif 