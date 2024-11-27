#ifndef WIRED_WIRED_H
#define WIRED_WIRED_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja.h"

void wired_mode_task(uint32_t timestamp);

bool wired_mode_start(gamepad_mode_t mode);

#endif