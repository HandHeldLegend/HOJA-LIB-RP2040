#ifndef TRANSPORT_WLAN_H
#define TRANSPORT_WLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"

void transport_wlan_stop();
bool transport_wlan_init(core_params_s *params);
void transport_wlan_task(uint64_t timestamp);

#endif