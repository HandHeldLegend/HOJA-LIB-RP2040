#ifndef WIRED_NESBUS_H
#define WIRED_NESBUS_H

#include <stdint.h>
#include <stdbool.h>

bool nesbus_wired_start();
void nesbus_wired_task(uint32_t timestamp);

#endif
