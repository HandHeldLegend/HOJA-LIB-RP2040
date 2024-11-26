#ifndef USB_DS4INPUT_H
#define USB_DS4INPUT_H

#include "hoja_includes.h"

#define REPORT_ID_DS4 0x01

extern const tusb_desc_device_t ds4_device_descriptor;
extern const uint8_t ds4_hid_report_descriptor[];
extern const uint8_t ds4_configuration_descriptor[];

void ds4_hid_report(button_data_s *button_data, a_data_s *analog_data);

#endif