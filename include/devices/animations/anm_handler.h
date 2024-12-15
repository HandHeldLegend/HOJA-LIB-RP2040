#ifndef ANM_HANDLER_H
#define ANM_HANDLER_H
#include <stdint.h>

void anm_handler_setup_mode(uint8_t rgb_mode, uint16_t brightness);
void anm_handler_tick();

#endif