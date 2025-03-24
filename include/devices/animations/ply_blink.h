#ifndef PLY_BLINK_H
#define PLY_BLINK_H
#include "devices_shared_types.h"
#include <stdbool.h>

bool ply_blink_handler_ss(rgb_s *output, uint8_t size, rgb_s set_color);
bool ply_blink_handler(rgb_s *output, uint8_t size, rgb_s set_color);

#endif