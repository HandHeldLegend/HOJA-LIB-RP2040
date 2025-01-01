#include "wired/gamecube.h"
#include "hoja_bsp.h"
#include "board_config.h"

#if defined(HOJA_JOYBUS_GC_DRIVER) && (HOJA_JOYBUS_GC_DRIVER == JOYBUS_GC_DRIVER_HAL)
    #include "hal/joybus_gc_hal.h"
#endif

void gamecube_wired_task(uint32_t timestamp)
{
  #if defined(HOJA_JOYBUS_GC_TASK)
  HOJA_JOYBUS_GC_TASK(timestamp);
  #endif
}

bool gamecube_wired_start()
{
  #if defined(HOJA_JOYBUS_GC_INIT)
  HOJA_JOYBUS_GC_INIT();
  #endif
  return true;
}
