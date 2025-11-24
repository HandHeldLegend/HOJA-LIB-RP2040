#include "input/hover.h"
#include "utilities/settings.h"

uint8_t _hover_slot_reader_idx[36] = {0};
uint32_t _hover_scalers[36] = {0};
uint8_t _used_hover_slots = 0;
bool _hover_calibration_active[36] = {0};

typedef struct 
{
    uint16_t inputs[36];
} hover_data_s;

#define HOVER_DEFAULT_DEADZONE 128

// Calculate scaler when deadzone or range changes
// Returns 16.16 fixed point scaler
uint32_t _hover_calculate_scaler(uint16_t deadzone, uint16_t max_value, uint16_t target_range) {
    uint32_t effective_range = max_value - deadzone;
    return ((uint32_t)target_range << 8) / effective_range;  // One-time expensive divide
}

// Fast real-time scaling using the pre-calculated scaler
uint16_t _hover_scale_input(uint16_t input, uint16_t deadzone, uint32_t scaler) {
    if (input < deadzone) 
        return 0;

    uint32_t adjusted = (uint32_t)input - deadzone;
    uint32_t scaled   = (adjusted * scaler) >> 8;

    if (scaled > 4095) 
        scaled = 4095;

    return (uint16_t)scaled;
}


void hover_calibrate_start()
{
    hover_cfg_s *cfg;

    // Reset all calibrations
    for(int i = 0; i < _used_hover_slots; i++)
    {
        cfg = &hover_config->config[_hover_slot_reader_idx[i]];

        cfg->invert = 0;
        cfg->max = 0x000;
        cfg->min = 0xFFF;
    }
}

void hover_init()
{
    _used_hover_slots = 0;

    for(int i = 0; i < 36; i++)
    {
        if(input_static.input_info[i].input_type == MAPPER_INPUT_TYPE_HOVER)
        {
            _hover_slot_reader_idx[_used_hover_slots] = i;
            _used_hover_slots++;
        }
    }
}

void hover_task(uint64_t timestamp)
{

}