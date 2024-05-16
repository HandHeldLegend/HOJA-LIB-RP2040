#ifndef ANALOG_H
#define ANALOG_H

#include "hoja_includes.h"
#include "interval.h"

void analog_get_octoangle_data(uint8_t *axis, uint8_t *octant);
void analog_get_subangle_data(uint8_t *axis, uint8_t *octant);
void analog_send_reset();
void analog_init(a_data_s *in, a_data_s *out, a_data_s *desnapped, button_data_s *buttons);
void analog_calibrate_start();
void analog_calibrate_stop();
void analog_calibrate_save();
void analog_calibrate_angle();
void analog_calibrate_center();
void analog_task(uint32_t timestamp);

#endif