#ifndef WIRED_WIRED_H
#define WIRED_WIRED_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja.h"

bool wired_mode_task(uint64_t timestamp);

bool wired_mode_start(gamepad_mode_t mode);

void wired_mode_stop();

#endif