#ifndef DEVICES_RGB_H
#define DEVICES_RGB_H

#include <stdint.h>
#include <stdbool.h>
#include "utilities/callback.h"
#include "board_config.h"

#define RGB_TASK_INTERVAL (1000000/RGB_DRIVER_REFRESHRATE)

void rgb_set_player(uint8_t player);

void rgb_deinit(callback_t cb);
void rgb_flash(uint32_t color);
void rgb_flash_multi(uint32_t *colors, uint8_t size);
void rgb_indicate(uint32_t color);

void rgb_init(int mode, int brightness);
void rgb_task(uint32_t timestamp);

#endif
