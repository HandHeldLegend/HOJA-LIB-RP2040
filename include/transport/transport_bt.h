#ifndef TRANSPORT_BT_H
#define TRANSPORT_BT_H

#include <stdbool.h>
#include <stdint.h>

bool transport_bt_init();
void transport_bt_task(uint64_t timestamp);

#endif