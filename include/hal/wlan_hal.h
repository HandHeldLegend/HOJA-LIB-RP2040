#ifndef WLAN_HAL_H
#define WLAN_HAL_H

#include "board_config.h"
#include "devices/wlan.h"
#include <stdint.h>
#include <stdbool.h>

#if defined(HOJA_WLAN_DRIVER) && (HOJA_WLAN_DRIVER==WLAN_DRIVER_HAL)

#define HOJA_WLAN_INIT(device_mode, pairing_mode) wlan_hal_init(device_mode, pairing_mode)
#define HOJA_WLAN_TASK(timestamp) wlan_hal_task(timestamp)

bool wlan_hal_init(int device_mode, bool pairing_mode);
bool wlan_hal_task(uint64_t timestamp);

#endif
#endif