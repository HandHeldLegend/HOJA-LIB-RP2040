#include "devices/animations/anm_idle.h"
#include "devices/rgb.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)
bool anm_idle_get_state(rgb_s *output)
{
    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        output[i].color = 0x00;
    }
    return true;
}
bool anm_idle_handler(rgb_s* output)
{
    // do nothing
    return true;
}
#endif