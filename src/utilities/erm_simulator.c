#include "utilities/erm_simulator.h"
#include "utilities/interval.h"
#include "utilities/pcm.h"

#include "switch/switch_haptics.h"

#include <stdbool.h>
#include <math.h>    

// Utility macros
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Motor characteristics
#define MOTOR_MAX_FREQUENCY 160   // Hz - typical ERM motor max frequency
#define MOTOR_MIN_FREQUENCY 120    // Hz - lower bound where vibration becomes noticeable

// Lookup table sizes
#define FREQUENCY_LUT_SIZE 256
#define AMPLITUDE_LUT_SIZE 256

// Fixed point precision for calculations
#define FIXED_POINT_SHIFT 8
#define FIXED_POINT_SCALE (1 << FIXED_POINT_SHIFT)

volatile int32_t _target_intensity = 0;
volatile int32_t _current_intensity = 0;

// Step values
static uint16_t     _frequency_min = 0;
static uint16_t     _frequency_step = 0;
static uint16_t     _amplitude_step = 0;

// Set desired motor intensity
void erm_simulator_set_intensity(uint8_t intensity) {

    intensity = intensity > 255 ? 255 : intensity;

    if((int32_t) intensity == _target_intensity) return;

    _target_intensity = (int32_t) intensity;
}

// Update motor simulation state - call this at MOTOR_UPDATE_RATE_HZ
bool _motor_sim_update(uint16_t *out_freq, uint16_t *out_amp) {
    uint16_t freq = 0;
    uint16_t amp = 0;
    
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

    // Clamp intensity to 0-AMPLITUDE_LUT_SIZE
    _current_intensity = (_current_intensity < _target_intensity) ? _target_intensity : _current_intensity;
    _current_intensity = (_current_intensity > _target_intensity) ? _target_intensity : _current_intensity;

    switch_haptics_get_basic(_current_intensity, 64, out_amp, out_freq);

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

    static uint16_t     freq_step;
    static uint16_t     amp_fixed;
    static haptic_processed_s pcm_data = {0};

    if(interval_run(timestamp, 16000, &interval))
    {
        bool update = _motor_sim_update(&freq_step, &amp_fixed);

        pcm_data.lo_amplitude_fixed = amp_fixed;
        pcm_data.hi_amplitude_fixed = amp_fixed;
        pcm_data.lo_frequency_increment = freq_step;
        pcm_data.hi_frequency_increment = freq_step;

        if(update) pcm_amfm_push(&pcm_data);
    }
}