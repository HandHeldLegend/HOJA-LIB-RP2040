#ifndef TRIGGERS_H
#define TRIGGERS_H

#include "hoja_includes.h"

void triggers_scale(int left_trigger_in, int *left_trigger_out, int right_trigger_in, int *right_trigger_out);
void triggers_scale_init();
void triggers_stop_calibration();
void triggers_start_calibration();

#endif