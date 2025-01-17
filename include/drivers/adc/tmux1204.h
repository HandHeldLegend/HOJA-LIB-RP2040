#ifndef DRIVERS_ADC_TMUX1204_H
#define DRIVERS_ADC_TMUX1204_H

#include "board_config.h"
#include <stdbool.h>

bool        tmux1204_register_driver(uint8_t ch_local, adc_driver_cfg_s *cfg);
uint16_t    tmux1204_read(uint8_t ch_local, uint8_t driver_instance);

#endif