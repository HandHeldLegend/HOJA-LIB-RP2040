#ifndef INPUT_SNAPBACK_H
#define INPUT_SNAPBACK_H

#include <stdint.h>
#include "input_shared_types.h"

#define ARC_MAX_WIDTH 10
#define TRIGGER_MAX_WIDTH (ARC_MAX_WIDTH/2)
#define ARC_MIN_WIDTH 20

#define CENTERVAL 2048
#define MAXVAL 4095
#define ARC_MAX_HEIGHT 200
#define INPUT_BUFFER_MAX (TRIGGER_MAX_WIDTH+2)
#define ARC_MIN_HEIGHT 1850

void snapback_process(analog_data_s *input, analog_data_s *output);
void snapback_webcapture_task(uint32_t timestamp, analog_data_s *data);
void snapback_debug_dump_webusb(uint16_t distance, bool crossover);

#endif