#ifndef USB_DINPUT_H
#define USB_DINPUT_H

#include "input/button.h"
#include "input/analog.h"
#include "tusb.h"

extern const tusb_desc_device_t di_device_descriptor;
extern const uint8_t di_hid_report_descriptor[];
extern const uint8_t di_configuration_descriptor[];

void dinput_hid_report(button_data_s *button_data, analog_data_s *analog_data);

#endif
