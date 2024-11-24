#ifndef HOJA_JOYBUS_N64_HAL_H
#define HOJA_JOYBUS_N64_HAL_H

void joybus_n64_hal_update_input(button_data_s *buttons, a_data_s *analog);

void joybus_n64_hal_task();

void joybus_n64_hal_init();

void joybus_n64_hal_deinit();

#endif
