#include "input/stick_deadzone.h"
#include "utilities/settings.h"
#include "input/analog.h"

// Calculate scaler when deadzone or range changes
// Returns 16.16 fixed point scaler
uint32_t _calculate_scaler(uint16_t deadzone, uint16_t max_range) {
    uint32_t effective_range = max_range - deadzone;
    return ((uint32_t)max_range << 8) / effective_range;  // One-time expensive divide
}

// Fast real-time scaling using the pre-calculated scaler
uint32_t _scale_input(uint32_t input, uint16_t deadzone, uint32_t scaler) {
    if (input <= deadzone) return 0;

    uint32_t adjusted = input - deadzone;
    return (adjusted * scaler) >> 8;  // Just multiply and shift
}

int16_t _process_deadzone(analog_meta_s *meta, int16_t deadzone)
{
    if(meta->distance <= deadzone) return 0;

    uint32_t scaler = _calculate_scaler(deadzone, meta->target);

    // Use fixed point to scale
    uint16_t output = (uint16_t) _scale_input(meta->distance, deadzone, scaler);
    if(output > meta->target) output = meta->target;

    return output;
}

void stick_deadzone_process(analog_data_s *in, analog_data_s *out)
{
    // Copy input data
    int16_t inl[2] = {in->lx, in->ly};
    int16_t outl[2] = {0};
    analog_meta_s lmeta = {.angle = in->langle, .distance = in->ldistance, .target = in->ltarget};

    int16_t inr[2] = {in->rx, in->ry};
    int16_t outr[2] = {0};
    analog_meta_s rmeta = {.angle = in->rangle, .distance = in->rdistance, .target = in->rtarget};

    int16_t l_dist_new = _process_deadzone(&lmeta, analog_config->l_deadzone);
    int16_t r_dist_new = _process_deadzone(&rmeta, analog_config->r_deadzone);

    if(l_dist_new)
        analog_angle_distance_to_coordinate(in->langle, l_dist_new, outl);

    if(r_dist_new)    
        analog_angle_distance_to_coordinate(in->rangle, r_dist_new, outr);

    // Copy data to output
    out->lx = outl[0];
    out->ly = outl[1];

    out->rx = outr[0];
    out->ry = outr[1];
}