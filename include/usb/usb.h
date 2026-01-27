/*
 * Copyright (c) [2023] [Mitch Cairns/Handheldlegend, LLC]
 * All rights reserved.
 *
 * This source code is licensed under the provisions of the license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef USB_HUSB_H
#define USB_HUSB_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja.h"

// Return value represents if we sent a packet
bool usb_mode_task(uint64_t timestamp);
void usb_mode_stop();
bool usb_mode_start(gamepad_mode_t mode);

#endif
