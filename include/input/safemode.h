#ifndef SAFEMODE_H
#define SAFEMODE_H

#include "hoja_includes.h"

void safe_mode_task(uint32_t timestamp, button_data_s *in);
bool safe_mode_check();

#endif