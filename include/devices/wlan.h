#ifndef DEVICES_WLAN_H
#define DEVICES_WLAN_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"

bool wlan_mode_start(gamepad_mode_t mode, bool pairing_mode);
bool wlan_mode_task(uint64_t timestamp);


#endif