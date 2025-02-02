#ifndef INPUT_BUTTON_H
#define INPUT_BUTTON_H

#include "input_shared_types.h"
#include <stdbool.h>

typedef enum
{
  DI_HAT_TOP          = 0x00,
  DI_HAT_TOP_RIGHT    = 0x01,
  DI_HAT_RIGHT        = 0x02,
  DI_HAT_BOTTOM_RIGHT = 0x03,
  DI_HAT_BOTTOM       = 0x04,
  DI_HAT_BOTTOM_LEFT  = 0x05,
  DI_HAT_LEFT         = 0x06,
  DI_HAT_TOP_LEFT     = 0x07,
  DI_HAT_CENTER       = 0x08,
} di_input_hat_dir_t; // Direct Input HAT values

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
} xi_input_hat_dir_t; // XInput HAT values

typedef enum
{
    HAT_MODE_DI,
    HAT_MODE_XI,
} hat_mode_t;

typedef enum 
{
    BUTTON_ACCESS_RAW_DATA,
    BUTTON_ACCESS_BOOT_DATA
} button_access_t;

bool button_init();
void button_access_blocking(button_data_s *out, button_access_t type);
void button_access_safe(button_data_s *out, button_access_t type);

void button_task(uint32_t timestamp);

#endif