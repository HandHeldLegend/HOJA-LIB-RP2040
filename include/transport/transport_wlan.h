#ifndef TRANSPORT_WLAN_H
#define TRANSPORT_WLAN_H

#include <stdbool.h>
#include <stdint.h>

bool transport_wlan_init();
void transport_wlan_task(uint64_t timestamp);

#endif