#ifndef SWPRO_H
#define SWPRO_H

#define REPORT_ID_SWITCH_INPUT 0x30
#define REPORT_ID_SWITCH_CMD 0x21
#define REPORT_ID_SWITCH_INIT 0x81

#include "hoja_includes.h"

extern const tusb_desc_device_t swpro_device_descriptor;
extern const uint8_t swpro_hid_report_descriptor[];
extern const uint8_t swpro_configuration_descriptor[];

void swpro_hid_report(button_data_s *button_data, a_data_s *analog_data);

#endif
