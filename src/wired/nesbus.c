#include "wired/nesbus.h"
#include "hoja_bsp.h"
#include "board_config.h"
#include "input/mapper.h"

#include <string.h>

#if defined(HOJA_NESBUS_DRIVER) && (HOJA_NESBUS_DRIVER == NESBUS_DRIVER_HAL)
#include "hal/nesbus_hal.h"
#endif

typedef struct
{
  uint8_t r : 1;
  uint8_t l : 1;
  uint8_t x : 1;
  uint8_t a : 1;
  uint8_t dright : 1;
  uint8_t dleft : 1;
  uint8_t ddown : 1;
  uint8_t dup : 1;
  uint8_t start : 1;
  uint8_t select : 1;
  uint8_t y : 1;
  uint8_t b : 1;
} nesbus_input_s;

volatile bool _nesbus_input_ready = false;
nesbus_input_s _nesbus_input = {0};

typedef struct
{
  mapper_check_cb r;
  mapper_check_cb l;
  mapper_check_cb x;
  mapper_check_cb a;
  mapper_check_cb right;
  mapper_check_cb left;
  mapper_check_cb down;
  mapper_check_cb up;
  mapper_check_cb start;
  mapper_check_cb select;
  mapper_check_cb y;
  mapper_check_cb b;
} nesbus_remap_checks_s;

void _nesbus_update_unmapped(bool down)
{
  (void) down;
}

void _nesbus_update_r(bool down)
{
  _nesbus_input.r |= down;
}

void _nesbus_update_l(bool down)
{
  _nesbus_input.l |= down;
}

void _nesbus_update_x(bool down)
{
  _nesbus_input.x |= down;
}

void _nesbus_update_a(bool down)
{
  _nesbus_input.a |= down;
}

void _nesbus_update_dleft(bool down)
{
  _nesbus_input.dleft |= down;
}

void _nesbus_update_dright(bool down)
{
  _nesbus_input.dright |= down;
}

void _nesbus_update_dup(bool down)
{
  _nesbus_input.dup |= down;
}

void _nesbus_update_ddown(bool down)
{
  _nesbus_input.ddown |= down;
}

void _nesbus_update_start(bool down)
{
  _nesbus_input.start |= down;
}

void _nesbus_update_select(bool down)
{
  _nesbus_input.select |= down;
}

void _nesbus_update_y(bool down)
{
  _nesbus_input.y |= down;
}

void _nesbus_update_b(bool down)
{
  _nesbus_input.b |= down;
}

void _nesbus_update_end(bool down)
{
  _nesbus_input_ready = true;
}

void _nesbus_set_remap()
{

}

bool nesbus_wired_start()
{
#if defined(HOJA_NESBUS_INIT)

  // Set up remaps

  HOJA_NESBUS_INIT();
#endif
  return true;
}

void nesbus_wired_task(uint64_t timestamp)
{
  memset(&_nesbus_input, 0, sizeof(nesbus_input_s));
  mapper_process();
#if defined(HOJA_NESBUS_TASK)
  HOJA_NESBUS_TASK(timestamp);
#endif
}
