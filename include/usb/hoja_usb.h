/*
 * Copyright (c) [2023] [Mitch Cairns/Handheldlegend, LLC]
 * All rights reserved.
 *
 * This source code is licensed under the provisions of the license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef HOJA_USB_H
#define HOJA_USB_H

#include "hoja_includes.h"
#include "interval.h"

bool hoja_usb_start(input_mode_t mode);
uint8_t dir_to_hat(hat_mode_t hat_type, uint8_t leftRight, uint8_t upDown);
void hoja_usb_task(uint32_t timestamp, button_data_s *button_data, a_data_s *analog_data);
void hoja_usb_set_usb_clear();

#endif
