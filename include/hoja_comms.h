#ifndef HOJA_COMMS_H
#define HOJA_COMMS_H

#include "devices/devices.h"
#include "input/analog.h"
#include "input/button.h"

typedef void (*comms_cb_t)(uint32_t, button_data_s *, analog_data_s *);

device_mode_t hoja_comms_current_mode();
void hoja_comms_task(uint32_t timestamp, button_data_s * buttons, analog_data_s * analog);
void hoja_comms_init(device_mode_t input_mode, device_method_t input_method);

#endif