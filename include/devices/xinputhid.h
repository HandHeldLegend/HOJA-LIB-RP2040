#ifndef XNPUTHID_H
#define XNPUTHID_H

#include "hoja_includes.h"

extern const tusb_desc_device_t xhid_device_descriptor;
extern const uint8_t xhid_hid_report_descriptor[];
extern const uint8_t xhid_configuration_descriptor[];

void xhid_hid_report(button_data_s *button_data, a_data_s *analog_data);


#endif // XNPUTHID_H