#include "input/snapback/snapback_lpf.h"
#include "input/snapback/snapback_utils.h"
#include "input/stick_scaling.h"
#include "utilities/settings.h"

#include <stdint.h>
#include <math.h>

#define Q16_SHIFT 16
#define Q16_ONE   (1 << Q16_SHIFT)

// --- NEW CONFIGURABLE PARAMETERS ---
#define BLEND_TIME_US 4000.0f  // Time in microseconds to fully transition (e.g., 4ms)
#define SNAPBACK_ZONE_RADIUS 1400 // Radius of the "Filter Zone" (signed range -2048 to 2048)

static inline int32_t _lpf_float_to_q16(float x) {
    return (int32_t)(x * (1 << Q16_SHIFT) + 0.5f);
}

static inline int32_t _lpf_q16_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> Q16_SHIFT);
}

typedef struct {
    int32_t alpha_q16;       
    int32_t prev_output_q16; 
    uint16_t cutoff;
    float blend_factor;      // 0.0 (Raw) to 1.0 (Filtered)
    float blend_step;        // Amount to change per update
    float blend_step_up;
} FixedLPF;

FixedLPF lpf[4] = {0};

void _lpf_init(FixedLPF* filter, uint32_t cutoff_tenths_hz, uint32_t interval_us) {
    float cutoff_hz = cutoff_tenths_hz / 10.0f;
    float dt = (float)interval_us / 1e6f;
    float rc = 1.0f / (2.0f * 3.14159265f * cutoff_hz);
    float alpha = dt / (rc + dt);

    filter->alpha_q16 = _lpf_float_to_q16(alpha);
    filter->prev_output_q16 = 0;
    
    // Calculate linear step size based on BLEND_TIME_US
    filter->blend_step = (float)interval_us / BLEND_TIME_US;
    filter->blend_step_up = filter->blend_step * 0.3f;
    filter->blend_factor = 1.0f; // Default to filtered
}

// Returns the filtered value in Q16
int32_t lpf_get_filtered_q16(FixedLPF* filter, int32_t input) {
    int32_t input_q16 = input << Q16_SHIFT;
    int32_t delta = input_q16 - filter->prev_output_q16;
    filter->prev_output_q16 += _lpf_q16_mul(filter->alpha_q16, delta);
    return filter->prev_output_q16;
}

void snapback_lpf_process(analog_axis_s *input, analog_axis_s *output) {
    if (input->axis_idx > 1) {
        *output = *input;
        return;
    }

    int l_idx = (input->axis_idx == 0) ? 0 : 2;
    uint32_t intensity = (input->axis_idx == 0) ? 
                         analog_config->l_snapback_intensity : 
                         analog_config->r_snapback_intensity;

    // Re-init logic
    if (lpf[l_idx].cutoff != intensity) {
        intensity = (intensity > 1500) ? 1500 : (intensity < 300 ? 300 : intensity);
        _lpf_init(&lpf[l_idx], intensity, 500);
        _lpf_init(&lpf[l_idx + 1], intensity, 500);
        lpf[l_idx].cutoff = intensity;
    }

    // 1. Determine if we are in the snapback zone
    // If inside radius, we want blend -> 1.0 (Filtered). Outside, blend -> 0.0 (Raw).
    bool in_zone = (input->distance < SNAPBACK_ZONE_RADIUS);

    // 2. Linearly update the blend factor
    if (in_zone) {
        lpf[l_idx].blend_factor += lpf[l_idx].blend_step_up;
        if (lpf[l_idx].blend_factor > 1.0f) lpf[l_idx].blend_factor = 1.0f;
    } else {
        lpf[l_idx].blend_factor -= lpf[l_idx].blend_step;
        if (lpf[l_idx].blend_factor < 0.0f) lpf[l_idx].blend_factor = 0.0f;
    }

    // 3. Process the filter and blend with the raw input
    int32_t filtered_x_q16 = lpf_get_filtered_q16(&lpf[l_idx], input->x);
    int32_t filtered_y_q16 = lpf_get_filtered_q16(&lpf[l_idx + 1], input->y);

    float raw_weight = 1.0f - lpf[l_idx].blend_factor;
    float filtered_weight = lpf[l_idx].blend_factor;

    output->x = (int16_t)((input->x * raw_weight) + ((filtered_x_q16 >> Q16_SHIFT) * filtered_weight));
    output->y = (int16_t)((input->y * raw_weight) + ((filtered_y_q16 >> Q16_SHIFT) * filtered_weight));

    // 4. Update metadata
    output->distance = (uint16_t)stick_scaling_coordinate_distance((int)output->x, (int)output->y);
    output->angle = stick_scaling_coordinates_to_angle(output->x, output->y);
    output->target = input->target;
    output->axis_idx = input->axis_idx;
}