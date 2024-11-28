#ifndef INPUT_BUTTON_H
#define INPUT_BUTTON_H

#include "input_shared_types.h"
#include <stdbool.h>

typedef enum
{
  NS_HAT_TOP          = 0x00,
  NS_HAT_TOP_RIGHT    = 0x01,
  NS_HAT_RIGHT        = 0x02,
  NS_HAT_BOTTOM_RIGHT = 0x03,
  NS_HAT_BOTTOM       = 0x04,
  NS_HAT_BOTTOM_LEFT  = 0x05,
  NS_HAT_LEFT         = 0x06,
  NS_HAT_TOP_LEFT     = 0x07,
  NS_HAT_CENTER       = 0x08,
} ns_input_hat_dir_t;

typedef enum
{
  XI_HAT_TOP          = 0x01,
  XI_HAT_TOP_RIGHT    = 0x02,
  XI_HAT_RIGHT        = 0x03,
  XI_HAT_BOTTOM_RIGHT = 0x04,
  XI_HAT_BOTTOM       = 0x05,
  XI_HAT_BOTTOM_LEFT  = 0x06,
  XI_HAT_LEFT         = 0x07,
  XI_HAT_TOP_LEFT     = 0x08,
  XI_HAT_CENTER       = 0x00,
} xi_input_hat_dir_t;

typedef enum
{
    HAT_MODE_NS,
    HAT_MODE_XI,
} hat_mode_t;

typedef enum 
{
    BUTTON_ACCESS_RAW_DATA,
    BUTTON_ACCESS_REMAPPED_DATA,
    BUTTON_ACCESS_BOOT_DATA
} button_access_t;

bool button_init();
void button_access_blocking(button_data_s *out, button_access_t type);
bool button_access_try(button_data_s *out, button_access_t type);

void button_task(uint32_t timestamp);

#endif