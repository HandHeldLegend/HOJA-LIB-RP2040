#include "wired/n64.h"
#include "hoja_bsp.h"
#include "board_config.h"

#if defined(HOJA_JOYBUS_N64_DRIVER) && (HOJA_JOYBUS_N64_DRIVER == JOYBUS_N64_DRIVER_HAL)
    #include "hal/joybus_n64_hal.h"
#endif

void n64_wired_task(uint32_t timestamp)
{
  #if defined(HOJA_JOYBUS_N64_TASK)
  HOJA_JOYBUS_N64_TASK(timestamp);
  #endif
}

bool n64_wired_start()
{
  #if defined(HOJA_JOYBUS_N64_INIT)
  HOJA_JOYBUS_N64_INIT();
  #endif
  return true;
}
