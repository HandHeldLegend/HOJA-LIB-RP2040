#ifndef SWPRO_H
#define SWPRO_H

#include "hoja_includes.h"

extern const tusb_desc_device_t swpro_device_descriptor;
extern const uint8_t swpro_hid_report_descriptor[];
extern const uint8_t swpro_configuration_descriptor[];

void swpro_hid_report(button_data_s *button_data, a_data_s *analog_data);

#endif
