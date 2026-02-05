#ifndef TRANSPORT_USB_H
#define TRANSPORT_USB_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"

void transport_usb_stop();
bool transport_usb_init(core_params_s *params);
void transport_usb_task(uint64_t timestamp);

#endif