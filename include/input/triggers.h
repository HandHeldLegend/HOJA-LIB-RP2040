#ifndef INPUT_TRIGGERS_H
#define INPUT_TRIGGERS_H

#include <stdbool.h>
#include <stdint.h>

void triggers_set_disabled(bool left_right, bool disabled);
void triggers_scale(int left_trigger_in, int *left_trigger_out, int right_trigger_in, int *right_trigger_out);
void triggers_scale_init();
void triggers_stop_calibration();
void triggers_start_calibration();

#endif