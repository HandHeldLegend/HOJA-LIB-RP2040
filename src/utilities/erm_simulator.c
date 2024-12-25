#include "utilities/erm_simulator.h"
#include "utilities/interval.h"
#include "utilities/pcm.h"

#include <stdbool.h>
#include <math.h>    

// Utility macros
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Motor characteristics
#define MOTOR_MAX_FREQUENCY 144   // Hz - typical ERM motor max frequency
#define MOTOR_MIN_FREQUENCY 25    // Hz - lower bound where vibration becomes noticeable

// Lookup table sizes
#define FREQUENCY_LUT_SIZE 256
#define AMPLITUDE_LUT_SIZE 256

// Fixed point precision for calculations
#define FIXED_POINT_SHIFT 8
#define FIXED_POINT_SCALE (1 << FIXED_POINT_SHIFT)

int32_t _target_intensity = 0;
int32_t _current_intensity = 0;
int32_t _highest_target_current = 0;
bool _highest_target_set = false;

// Lookup tables for motor response curves
static uint16_t frequency_lut[FREQUENCY_LUT_SIZE] = {
    // Will be initialized with precalculated values
    // Maps intensity (0-255) to frequency values
    // Format: fixed_increment values ready for PCM generation
};

static int16_t amplitude_lut[AMPLITUDE_LUT_SIZE] = {
    // Will be initialized with precalculated values
    // Maps intensity (0-255) to amplitude values
    // Format: fixed_amplitude values ready for PCM generation
};

// Set desired motor intensity
void erm_simulator_set_intensity(uint8_t intensity) {
    if((int32_t) intensity == _target_intensity) return;

    if((uint32_t) intensity > _highest_target_current)
    {
        _highest_target_current = (uint32_t) intensity;
        _highest_target_set = true;
    }

    _target_intensity = (int32_t) intensity;
}

// Update motor simulation state - call this at MOTOR_UPDATE_RATE_HZ
bool _motor_sim_update(uint16_t *out_freq, int16_t *out_amp) {
    uint16_t freq = 0;
    int16_t amp = 0;
    
    // Calculate ramp step size based on update rate and ramp time
    int32_t ramp_step = 3;
    int32_t fall_step = 6;
    bool change = false;

    if(_highest_target_set)
    {
        if(_current_intensity < _highest_target_current)
        {
            _current_intensity += ramp_step;
            change = true;
        }
        else 
        {
            _current_intensity = _highest_target_current;
            _highest_target_current = 0;
            _highest_target_set = false;
        }
    }
    else if(_current_intensity > _target_intensity) {
        _current_intensity -= fall_step;
        change = true;
    }

    // Clamp intensity to 0-AMPLITUDE_LUT_SIZE
    _current_intensity = (_current_intensity < 0) ? 0 : _current_intensity;
    _current_intensity = (_current_intensity > 255) ? 255 : _current_intensity;
        
    // Look up frequency and amplitude from tables
    freq    = frequency_lut[_current_intensity];
    amp     = amplitude_lut[_current_intensity];
    
    *out_freq   = freq;
    *out_amp    = amp;

    return change;
}

// Utility function to generate lookup tables
void _generate_motor_lookup_tables(void) {
    float freq, amp;
    uint16_t fixed_freq;
    int16_t fixed_amp;

    // Frequency step size 
    float f_step = (MOTOR_MAX_FREQUENCY - MOTOR_MIN_FREQUENCY) / (FREQUENCY_LUT_SIZE - 1);
    freq = MOTOR_MIN_FREQUENCY;

    float a_step = 1.0f / (AMPLITUDE_LUT_SIZE - 1);
    amp = 0.0f;
    
    for(int i = 0; i < FREQUENCY_LUT_SIZE; i++) {
        // Convert to PCM increment value
        freq = MOTOR_MIN_FREQUENCY + (i * f_step);
        fixed_freq = (uint16_t)((freq * PCM_SINE_TABLE_SIZE / PCM_SAMPLE_RATE) * 
                               PCM_FREQUENCY_SHIFT_FIXED + 0.5f);
                               
        frequency_lut[i] = fixed_freq;
        
        // Convert to PCM amplitude value
        if(i > 0) {
            amp = i * a_step;
            fixed_amp = (int16_t)(amp * PCM_AMPLITUDE_SHIFT_FIXED + 0.5f);
        }
        else {
            amp = 0.0f;
            fixed_amp = 0;
        }

        amplitude_lut[i] = fixed_amp;
    }

    amplitude_lut[0] = 0;
}

bool _erm_ready = false;

void erm_simulator_task(uint32_t timestamp)
{   
    interval_s interval = {0};

    static uint16_t freq_step;
    static int16_t  amp_fixed;
    static haptic_processed_s pcm_data = {0};

    if(!_erm_ready)
    {
        _generate_motor_lookup_tables();
        _erm_ready = true;
    }

    if(interval_run(timestamp, 4000, &interval))
    {
        bool update = _motor_sim_update(&freq_step, &amp_fixed);

        pcm_data.lo_amplitude_fixed = amp_fixed;
        pcm_data.hi_amplitude_fixed = 0;
        pcm_data.lo_frequency_increment = freq_step;
        pcm_data.hi_frequency_increment = 0;

        if(update) pcm_amfm_push(&pcm_data);
    }
}