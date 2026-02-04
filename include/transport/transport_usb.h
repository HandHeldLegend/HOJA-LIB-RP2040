#ifndef TRANSPORT_USB_H
#define TRANSPORT_USB_H

#include <stdbool.h>
#include <stdint.h>

bool transport_usb_init();
void transport_usb_task(uint64_t timestamp);

#endif