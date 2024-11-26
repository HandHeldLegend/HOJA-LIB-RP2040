#ifndef HOJA_JOYBUS_GC_HAL_H
#define HOJA_JOYBUS_GC_HAL_H

#include <stdint.h>
#include "input/button.h"
#include "input/analog.h"

// Update the joybus input data
void joybus_gc_hal_update_input(button_data_s *buttons, analog_data_s *analog);

void joybus_gc_hal_task();

void joybus_gc_hal_init();

void joybus_gc_hal_deinit();

#endif