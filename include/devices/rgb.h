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
#define RGB_MAX_GROUPS 32

extern int8_t rgb_led_groups[RGB_MAX_GROUPS][RGB_MAX_LEDS_PER_GROUP];
extern rgb_s  rgb_colors_safe[RGB_MAX_GROUPS];
#endif

void rgb_set_idle(bool enable); 
void rgb_deinit(callback_t cb);
void rgb_init(int mode, int brightness);
void rgb_task(uint64_t timestamp);

#endif
