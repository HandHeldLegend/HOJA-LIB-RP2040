#ifndef ANM_HANDLER_H
#define ANM_HANDLER_H
#include <stdint.h>
#include "utilities/callback.h"

void anm_handler_shutdown(callback_t cb);
void anm_set_idle_enable(bool enable);
void anm_handler_setup_mode(uint8_t rgb_mode, uint16_t brightness, uint32_t animation_time_ms);
void anm_handler_tick();

#endif