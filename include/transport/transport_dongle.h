#ifndef TRANSPORT_DONGLE_H
#define TRANSPORT_DONGLE_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"
#include "transport/transport_bt.h"

void transport_dongle_stop();
bool transport_dongle_init(core_params_s *params);
void transport_dongle_task(uint64_t timestamp);

// The dongle asked for another mode: switch to @p format with the link kept up (see hoja.c)
bool transport_dongle_take_mode(core_reportformat_t *format);

// Following the dongle and not adopted yet: the mode isn't settled (lights hold, as in Auto)
bool transport_dongle_choosing(void);

uint8_t transport_dongle_static_supported(void);
uint8_t transport_dongle_static_part_status(void);

#endif
