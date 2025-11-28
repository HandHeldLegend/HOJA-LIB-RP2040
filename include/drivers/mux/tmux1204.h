#ifndef DRIVERS_MUX_TMUX1204_H
#define DRIVERS_MUX_TMUX1204_H

#include "board_config.h"
#include <stdbool.h>
#include "driver_define_helper.h"

bool tmux1204_init(mux_tmux1204_driver_s *driver);
bool tmux1204_switch_channel(mux_tmux1204_driver_s *driver, uint8_t chan);

#endif