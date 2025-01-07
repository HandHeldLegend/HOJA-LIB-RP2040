#ifndef UTILITIES_ERM_SIMULATOR_H
#define UTILITIES_ERM_SIMULATOR_H
#include <stdint.h>

void erm_simulator_init();
void erm_simulator_set_intensity(uint8_t intensity);
void erm_simulator_task(uint32_t timestamp);

#endif 