#include "utilities/erm_simulator.h"
#include "utilities/interval.h"
#include "utilities/pcm.h"

#include "switch/switch_haptics.h"

#include <stdbool.h>
#include <math.h>    

volatile int32_t _target_intensity = 0;
volatile int32_t _current_intensity = 0;

// Step values
static uint16_t     _frequency_min = 0;
static uint16_t     _frequency_step = 0;
static uint16_t     _amplitude_step = 0;

#define ERM_SHIFT_VAL 8

// Set desired motor intensity
void erm_simulator_set_intensity(uint8_t intensity) {
    intensity = intensity > 220 ? 220 : intensity;
    intensity >>= 1;
    
    int32_t adjusted_intensity = intensity << ERM_SHIFT_VAL; // Shift left by 4 bits to get the 12-bit value

    if((int32_t) adjusted_intensity == _target_intensity) return;

    _target_intensity = (int32_t) adjusted_intensity;
}

// Update motor simulation state - call this at MOTOR_UPDATE_RATE_HZ
bool _motor_sim_update() {
    // Calculate ramp step size based on update rate and ramp time
    int32_t ramp_step = 1;
    int32_t fall_step = 3;
    bool change = false;

    if(_current_intensity < _target_intensity)
    {
        _current_intensity += ramp_step;
        change = true;
    }
    else if (_current_intensity > _target_intensity)
    {
        _current_intensity -= fall_step;
        change = true;
    }

    _current_intensity = (_current_intensity < _target_intensity) ? _target_intensity : _current_intensity;
    _current_intensity = (_current_intensity > _target_intensity) ? _target_intensity : _current_intensity;

    return change;
}

bool _erm_ready = false;

// Utility function to generate lookup tables
void erm_simulator_init() 
{
    return;
}

void erm_simulator_task(uint32_t timestamp)
{   
    interval_s interval = {0};

    static uint8_t intensity = 0;

    if(interval_run(timestamp, 16000, &interval))
    {
        bool update = _motor_sim_update();

        if(update) switch_haptics_arbitrary_playback(_current_intensity >> ERM_SHIFT_VAL);
    }
}