#ifndef INPUT_INPUT_H
#define INPUT_INPUT_H

#include <stdint.h>
#include <stdbool.h>

bool input_init();
void input_digital_task(uint32_t timestamp);
void input_analog_task(uint32_t timestamp);

#endif