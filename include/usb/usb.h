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

void usb_mode_task(uint32_t timestamp);

bool usb_mode_start(gamepad_mode_t mode);

uint8_t dir_to_hat(hat_mode_t hat_type, uint8_t leftRight, uint8_t upDown);

#endif
