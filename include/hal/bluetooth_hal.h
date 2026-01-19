#ifndef BLUETOOTH_HAL_H
#define BLUETOOTH_HAL_H

#include "board_config.h"
#include "devices/bluetooth.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_HAL)

#define HOJA_BLUETOOTH_INIT(device_mode, pairing_mode, evt_cb) bluetooth_hal_init(device_mode, pairing_mode, evt_cb)
#define HOJA_BLUETOOTH_TASK(timestamp) bluetooth_hal_task(timestamp)
#define HOJA_BLUETOOTH_GET_FWVERSION() bluetooth_hal_get_fwversion()
#define HOJA_BLUETOOTH_STOP() bluetooth_hal_stop()

void bluetooth_hal_stop();
bool bluetooth_hal_init(int device_mode, bool pairing_mode, bluetooth_cb_t evt_cb);
void bluetooth_hal_task(uint64_t timestamp);
uint32_t bluetooth_hal_get_fwversion(void);
#endif
#endif 