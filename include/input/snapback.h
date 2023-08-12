#ifndef SNAPBACK_H
#define SNAPBACK_H

#include "hoja_includes.h"
#include "interval.h"

void snapback_process(uint32_t timestamp, a_data_s *input, a_data_s *output);
void snapback_webcapture_task(uint32_t timestamp, a_data_s *data);

#endif