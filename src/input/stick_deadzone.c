#include "input/stick_deadzone.h"
#include "utilities/settings.h"
#include "input/analog.h"

#define Q16_ONE             (1u << 16)
#define Q8_ONE              (1u << 8)

// Calculate scaler when deadzone or range changes
// Returns 8.8 fixed point scaler
uint32_t _calculate_scaler(uint16_t deadzone, uint16_t outer_deadzone, uint16_t target) {
    uint32_t effective_range = target - outer_deadzone - deadzone;
    return ((uint32_t)target << 8) / effective_range;  // One-time expensive divide
}

// Fast real-time scaling using the pre-calculated scaler
uint32_t _scale_input(uint32_t input, uint16_t deadzone, uint32_t scaler) {
    if (input <= deadzone) return 0;

    uint32_t adjusted = input - deadzone;
    return (adjusted * scaler) >> 8;  // Just multiply and shift
}

static inline uint32_t _q16_mul(uint32_t a, uint32_t b)
{
    return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 16);
}

// Integer sqrt -> Q16 sqrt: sqrt(x / 65536) as Q16 ~= isqrt(x << 16)
static uint32_t _q16_sqrt(uint32_t x)
{
    uint64_t op = (uint64_t)x << 16;
    uint64_t res = 0;
    uint64_t one = 1ull << 62;

    while (one > op)
        one >>= 2;

    while (one != 0)
    {
        if (op >= res + one)
        {
            op -= res + one;
            res = (res >> 1) + one;
        }
        else
        {
            res >>= 1;
        }
        one >>= 2;
    }

    return (uint32_t)res;
}

// Convert offset-encoded expo (1..251) to Q8 (256 == 1.00)
static uint32_t _exp_stored_to_q8(uint8_t stored)
{
    if (stored < ANALOG_EXP_STORED_MIN)
        stored = ANALOG_EXP_STORED_DEFAULT;
    else if (stored > ANALOG_EXP_STORED_MAX)
        stored = ANALOG_EXP_STORED_MAX;

    // sensitivity = stored + 49; e_q8 = sensitivity * 256 / 100
    return ((uint32_t)(stored + ANALOG_EXP_SENSITIVITY_OFFSET) * Q8_ONE) / 100u;
}

// Compute x^e for x in Q16 (0..1) and e in Q8 (256 == 1.0) via integer power
// and successive square-roots for the fractional bits.
static uint32_t _q16_pow_q8(uint32_t x_q16, uint32_t e_q8)
{
    if (x_q16 == 0)
        return 0;
    if (e_q8 == Q8_ONE)
        return x_q16;
    if (x_q16 >= Q16_ONE)
        return Q16_ONE;

    uint32_t result = Q16_ONE;

    // Integer part: x^floor(e)
    uint32_t int_part = e_q8 >> 8;
    uint32_t x_pow = x_q16;
    while (int_part)
    {
        if (int_part & 1u)
            result = _q16_mul(result, x_pow);
        x_pow = _q16_mul(x_pow, x_pow);
        int_part >>= 1;
    }

    // Fractional part via successive square roots (bit7=0.5 ... bit0=1/256)
    uint32_t root = x_q16;
    for (int bit = 7; bit >= 0; bit--)
    {
        root = _q16_sqrt(root);
        if (e_q8 & (1u << bit))
            result = _q16_mul(result, root);
    }

    return result;
}

static uint16_t _apply_exp_curve(uint16_t distance, uint16_t target, uint8_t exp_stored)
{
    if (distance == 0 || target == 0)
        return 0;

    if (exp_stored == 0 || exp_stored == 0xFF)
        exp_stored = ANALOG_EXP_STORED_DEFAULT;

    // Unity curve: skip the power path
    if (exp_stored == ANALOG_EXP_STORED_DEFAULT)
        return distance > target ? target : distance;

    if (distance >= target)
        return target;

    uint32_t e_q8 = _exp_stored_to_q8(exp_stored);
    if (e_q8 == Q8_ONE)
        return distance;

    uint32_t x_q16 = ((uint32_t)distance << 16) / (uint32_t)target;
    uint32_t y_q16 = _q16_pow_q8(x_q16, e_q8);
    uint32_t output = (y_q16 * (uint32_t)target) >> 16;

    if (output > target)
        output = target;

    return (uint16_t)output;
}

int16_t _process_deadzone(analog_meta_s *meta, int16_t deadzone, int16_t outer_deadzone, uint8_t exp_stored)
{
    if(meta->distance <= deadzone) return 0;

    uint32_t scaler = _calculate_scaler(deadzone, outer_deadzone, meta->target);

    // Use fixed point to scale
    uint16_t output = (uint16_t) _scale_input(meta->distance, deadzone, scaler);
    if(output > meta->target) output = meta->target;

    // Exponential curve after deadzone remap: output = target * (d/target)^e
    output = _apply_exp_curve(output, meta->target, exp_stored);

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

    int16_t l_dist_new = _process_deadzone(&lmeta, analog_config->l_deadzone, analog_config->l_deadzone_outer, analog_config->l_exp_scaler);
    int16_t r_dist_new = _process_deadzone(&rmeta, analog_config->r_deadzone, analog_config->r_deadzone_outer, analog_config->r_exp_scaler);

    out->ldistance = l_dist_new;
    out->rdistance = r_dist_new;

    out->langle = in->langle;
    out->rangle = in->rangle;

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
