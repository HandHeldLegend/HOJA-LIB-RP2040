#ifndef USB_DINPUT_H
#define USB_DINPUT_H

#include "tusb.h"
#include <stdint.h>

extern const tusb_desc_device_t di_device_descriptor;
extern const uint8_t di_hid_report_descriptor[];
extern const uint8_t di_configuration_descriptor[];

void dinput_hid_report(uint32_t timestamp);

#endif
