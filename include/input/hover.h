#ifndef INPUT_HOVER_H
#define INPUT_HOVER_H

// This is HOJAs new input system called HOVER
// HOVER is designed to allow a dynamic array of inputs
// that can be analog or digital in nature. It also manages
// input thresholds, deadzone, and the ability to implement rapid trigger

#include <stdint.h>
#include <stdbool.h>

typedef struct 
{
    float scaler;
    uint16_t threshold;
    uint16_t delta; // The delta that the input must travel to change state
    bool pressed; // The state of the digital press
} hover_live_calibration_s;

typedef struct 
{
    
} hover_setup_s;

#endif