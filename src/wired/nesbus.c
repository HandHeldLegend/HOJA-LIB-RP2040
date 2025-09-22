#include "wired/nesbus.h"
#include "hoja_bsp.h"
#include "board_config.h"
#include "input/mapper.h"

#include <string.h>

#if defined(HOJA_NESBUS_DRIVER) && (HOJA_NESBUS_DRIVER == NESBUS_DRIVER_HAL)
#include "hal/nesbus_hal.h"
#endif

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
#if defined(HOJA_NESBUS_TASK)
  HOJA_NESBUS_TASK(timestamp);
#endif
}
