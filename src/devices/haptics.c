#include "devices/haptics.h"

#include "pico/multicore.h"
#include <string.h>

#include "hal/hdrumble_hal.h"
#include "utilities/erm_simulator.h"

float _rumble_intensity = 1.0f;
bool _erm_simulation = false;

void haptics_set_intensity(float intensity)
{
    _rumble_intensity = intensity;
}

float haptics_get_intensity()
{
    return _rumble_intensity;
}

void haptics_set_std(uint8_t amplitude)
{
    #if defined(HOJA_HAPTICS_SET_STD)
    HOJA_HAPTICS_SET_STD(amplitude);
    #endif
}

bool haptics_init() 
{
    #if defined(HOJA_HAPTICS_INIT)
    return HOJA_HAPTICS_INIT();
    #endif
}

void haptics_task(uint32_t timestamp)
{
    #if defined(HOJA_HAPTICS_TASK)
    HOJA_HAPTICS_TASK(timestamp);
    #endif
}
