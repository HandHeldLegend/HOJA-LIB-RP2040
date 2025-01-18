#ifndef DRIVERS_ADC_TMUX1204_H
#define DRIVERS_ADC_TMUX1204_H

#include "board_config.h"
#include <stdbool.h>

bool        tmux1204_init_channel(adc_channel_cfg_s *cfg);
uint16_t    tmux1204_read_channel(adc_channel_cfg_s *cfg);

#endif