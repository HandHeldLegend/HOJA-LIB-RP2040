#ifndef ANALOG_H
#define ANALOG_H

#include "hoja_includes.h"
#include "interval.h"

void analog_init(a_data_s *in, a_data_s *out, button_data_s *buttons);
void analog_calibrate_start();
void analog_calibrate_stop();
void analog_calibrate_save();
void analog_calibrate_angle();
void analog_calibrate_center();
void analog_task(uint32_t timestamp);

#endif