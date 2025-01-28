#include "devices/animations/anm_shutdown.h"
#include "devices/animations/anm_handler.h"
#include <stddef.h>
#include <string.h>

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

callback_t _shutdown_cb = NULL;
int _delay_steps = 10;

void anm_shutdown_set_cb(callback_t cb)
{
    _delay_steps = 10;
    _shutdown_cb = cb;
}

bool anm_shutdown_handler(rgb_s *output)
{
    memset(output, 0, sizeof(rgb_s)*RGB_DRIVER_LED_COUNT); // Set RGBs to OFF
    if(!_delay_steps) return false;

    _delay_steps--;
    if(!_delay_steps)
    {
        // Just shut down
        if(_shutdown_cb) _shutdown_cb();
    }

    return false;
}

bool anm_shutdown_get_state(rgb_s *output)
{
    memset(output, 0, sizeof(rgb_s)*RGB_DRIVER_LED_COUNT); // Set RGBs to OFF
}

#endif