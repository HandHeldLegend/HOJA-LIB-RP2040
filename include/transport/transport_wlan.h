#ifndef TRANSPORT_WLAN_H
#define TRANSPORT_WLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"
#include "transport/transport_bt.h"

void transport_wlan_stop();
bool transport_wlan_init(core_params_s *params);
void transport_wlan_task(uint64_t timestamp);

uint8_t transport_wlan_static_supported(void);
uint8_t transport_wlan_static_part_status(void);

#endif
