#include "wired/gamecube.h"
#include "hoja_bsp.h"
#include "board_config.h"

#if defined(HOJA_JOYBUS_GC_DRIVER) && (HOJA_JOYBUS_GC_DRIVER == JOYBUS_GC_DRIVER_HAL)
    #include "hal/joybus_gc_hal.h"
#endif

typedef enum 
{
    CORE_GAMECUBE_RUMBLE_ON,
    CORE_GAMECUBE_RUMBLE_OFF,
    CORE_GAMECUBE_CONNECTED, 
    CORE_GAMECUBE_DISCONNECTED,
} core_gamecube_evt_t;

void core_gamecube_cb(uint8_t *data, uint16_t len)
{

}

void gamecube_wired_task(uint64_t timestamp)
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
