#include "wired/nesbus.h"
#include "hoja_bsp.h"
#include "board_config.h"

#if defined(HOJA_NESBUS_DRIVER) && (HOJA_NESBUS_DRIVER == NESBUS_DRIVER_HAL)
    #include "hal/nesbus_hal.h"
#endif

bool nesbus_wired_start()
{
  #if defined(HOJA_NESBUS_INIT)
  HOJA_NESBUS_INIT();
  #endif
  return true;
}

void nesbus_wired_task(uint32_t timestamp)
{
  #if defined(HOJA_NESBUS_TASK)
  HOJA_NESBUS_TASK(timestamp);
  #endif
}
