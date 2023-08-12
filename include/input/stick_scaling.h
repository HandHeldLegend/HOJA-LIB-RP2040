#ifndef STICK_H
#define STICK_H

#include "hoja_includes.h"

void stick_scaling_get_settings();
void stick_scaling_set_settings();

void stick_scaling_init();

void stick_scaling_reset_distances();

bool stick_scaling_capture_distances(a_data_s *in);
void stick_scaling_capture_center(a_data_s *in);
bool stick_scaling_capture_angle(a_data_s *in);
void stick_scaling_process_data(a_data_s *in, a_data_s *out);

#endif
