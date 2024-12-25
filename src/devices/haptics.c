#include "devices/haptics.h"

#include "pico/multicore.h"
#include <string.h>

#include "hal/hdrumble_hal.h"
#include "utilities/erm_simulator.h"

float _rumble_intensity = 1.0f;

void haptics_set_intensity(float intensity)
{
    _rumble_intensity = intensity;
}

float haptics_get_intensity()
{
    return _rumble_intensity;
}

void haptics_set_basic(float amplitude) 
{

}

bool haptics_init() 
{
    #if HOJA_CONFIG_HDRUMBLE==1
    #warning "HDRUMBLE ENABLED"
    return hdrumble_hal_init();
    #endif
}

void haptics_task(uint32_t timestamp)
{
    #if HOJA_CONFIG_HDRUMBLE==1
    hdrumble_hal_task(timestamp);
    #endif

    erm_simulator_task(timestamp);
}
