#ifndef DINPUT_H
#define DINPUT_H

#include "hoja_includes.h"

extern const tusb_desc_device_t di_device_descriptor;
extern const uint8_t di_hid_report_descriptor[];
extern const uint8_t di_configuration_descriptor[];

void dinput_hid_report(button_data_s *button_data, a_data_s *analog_data);

#endif
