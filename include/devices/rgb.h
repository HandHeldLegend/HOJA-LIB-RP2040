#ifndef DEVICES_RGB_H
#define DEVICES_RGB_H

#include <stdint.h>
#include <stdbool.h>
#include "utilities/callback.h"
#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define RGB_TASK_INTERVAL (1000000/RGB_DRIVER_REFRESHRATE) 
#define RGB_FADE_FIXED_SHIFT 16
#define RGB_FADE_FIXED_MULT (uint32_t) (1<<RGB_FADE_FIXED_SHIFT)
#define RGB_FLOAT_TO_FIXED(f) ((uint32_t) (f * RGB_FADE_FIXED_MULT))

extern int8_t rgb_led_groups[HOJA_RGB_GROUPS_NUM][RGB_MAX_LEDS_PER_GROUP];
extern rgb_s  rgb_colors_safe[32];
#endif

void rgb_deinit(callback_t cb);
void rgb_init(int mode, int brightness);
void rgb_task(uint32_t timestamp);

#endif
