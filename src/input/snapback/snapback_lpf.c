#include "input/snapback/snapback_lpf.h"
#include "input/snapback/snapback_utils.h"
#include "input/stick_scaling.h"

#include "utilities/settings.h"

#include <stdint.h>
#include <math.h>

#define Q16_SHIFT 16
#define Q16_ONE   (1 << Q16_SHIFT)

// Convert float to Q16.16
static inline int32_t _lpf_float_to_q16(float x) {
    return (int32_t)(x * (1 << Q16_SHIFT) + 0.5f);
}

// Multiply two Q16.16 values
static inline int32_t _lpf_q16_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> Q16_SHIFT);
}

typedef struct {
    int32_t alpha_q16;       // Q16.16 filter coefficient
    int32_t prev_output_q16; // Q16.16 filtered value
    uint16_t cutoff;
} FixedLPF;

/**
 * Initialize LPF with:
 * @param cutoff_tenths_hz   - e.g. 105 = 10.5 Hz
 * @param interval_us        - sampling interval in microseconds
 */
void _lpf_init(FixedLPF* filter, uint32_t cutoff_tenths_hz, uint32_t interval_us) {
    float cutoff_hz = cutoff_tenths_hz / 10.0f;
    float dt = interval_us / 1e6f;
    float rc = 1.0f / (2.0f * 3.14159265f * cutoff_hz);
    float alpha = dt / (rc + dt);

    filter->alpha_q16 = _lpf_float_to_q16(alpha);
    filter->prev_output_q16 = 0;
}

FixedLPF lpf[4] = {0};

int32_t lpf_update(FixedLPF* filter, int32_t input) {
    int32_t input_q16 = input << Q16_SHIFT;
    int32_t delta = input_q16 - filter->prev_output_q16;
    int32_t filtered_delta = _lpf_q16_mul(filter->alpha_q16, delta);
    filter->prev_output_q16 += filtered_delta;
    return filter->prev_output_q16 >> Q16_SHIFT;
}

void snapback_lpf_process(analog_axis_s *input, analog_axis_s *output)
{
    if(!input->axis_idx)
    {
        // Check if we need to re-init 
        if(lpf[0].cutoff != analog_config->l_snapback_intensity)
        {
            if(analog_config->l_snapback_intensity > 1500)
            {
                // If intensity is too high, set to a default value
                analog_config->l_snapback_intensity = 1500; // 15.0 Hz
            }
            else if(analog_config->l_snapback_intensity < 300)
            {
                // If intensity is too low, set to a default value
                analog_config->l_snapback_intensity = 300; // 1.0 Hz
            }

            _lpf_init(&lpf[0], analog_config->l_snapback_intensity, 500);
            _lpf_init(&lpf[1], analog_config->l_snapback_intensity, 500);
            lpf[0].cutoff = analog_config->l_snapback_intensity;
        }

        // Process left axis
        output->x = lpf_update(&lpf[0], input->x);
        output->y = lpf_update(&lpf[1], input->y);
    }
    else if (input->axis_idx == 1)
    {
        // Check if we need to re-init 
        if(lpf[2].cutoff != analog_config->r_snapback_intensity)
        {
            if(analog_config->r_snapback_intensity > 1500)
            {
                // If intensity is too high, set to a default value
                analog_config->r_snapback_intensity = 1500; // 15.0 Hz
            }
            else if(analog_config->r_snapback_intensity < 200)
            {
                // If intensity is too low, set to a default value
                analog_config->r_snapback_intensity = 200; // 1.0 Hz
            }

            _lpf_init(&lpf[2], analog_config->r_snapback_intensity, 500);
            _lpf_init(&lpf[3], analog_config->r_snapback_intensity, 500);
            lpf[2].cutoff = analog_config->r_snapback_intensity;
        }

        // Process right axis
        output->x = lpf_update(&lpf[2], input->x);
        output->y = lpf_update(&lpf[3], input->y);
    }
    else
    {
        // Invalid axis index
        output->x = 0;
        output->y = 0;
    }

    float new_angle = stick_scaling_coordinates_to_angle(output->x, output->y);
    float new_distance = stick_scaling_coordinate_distance((int) output->x, (int) output->y);
    output->distance = (uint16_t) new_distance;
    output->angle = new_angle;
    output->target = input->target;
}