#ifndef TRANSPORT_WLAN_H
#define TRANSPORT_WLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"
#include "transport/transport_bt.h"

void transport_wlan_stop();
bool transport_wlan_init(core_params_s *params);
void transport_wlan_task(uint64_t timestamp);

// The dongle asked for another mode: switch to @p format with the link kept up (see hoja.c)
bool transport_wlan_take_mode(core_reportformat_t *format);

// Following the dongle and not adopted yet: the mode isn't settled (lights hold, as in Auto)
bool transport_wlan_choosing(void);

uint8_t transport_wlan_static_supported(void);
uint8_t transport_wlan_static_part_status(void);

#endif
