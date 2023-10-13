#ifndef HOJA_COMMS_H
#define HOJA_COMMS_H

#include "hoja_includes.h"

typedef void (*comms_cb_t)(uint32_t, button_data_s *, a_data_s *);

input_mode_t hoja_comms_current_mode();
void hoja_comms_task(uint32_t timestamp, button_data_s * buttons, a_data_s * analog);
void hoja_comms_init(input_mode_t input_mode, input_method_t input_method);

#endif