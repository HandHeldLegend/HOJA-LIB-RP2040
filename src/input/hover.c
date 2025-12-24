#include "hoja.h"
#include "input/hover.h"
#include "utilities/settings.h"
#include "utilities/crosscore_snapshot.h"
#include "input_shared_types.h"

SNAPSHOT_TYPE(hover, mapper_input_s);

snapshot_hover_t _hover_snap;

uint8_t _hover_slot_reader_idx[36] = {0};
uint32_t _hover_scalers[36] = {0};
uint8_t _used_hover_slots = 0;
uint8_t _calibration_ch_active = 0x00;

mapper_input_s _boot_capture = {0};

#define HOVER_DEFAULT_DEADZONE 128

// Calculate scaler when deadzone or range changes
// Q8 fixed-point scaler: scaled = (value * scaler) >> 8
uint32_t _hover_calculate_scaler(uint16_t min_value,
                                 uint16_t max_value,
                                 uint16_t deadzone,
                                 uint16_t target_range)
{
    // Effective deadzone is min + fixed deadzone
    uint32_t effective_deadzone = (uint32_t)min_value + (uint32_t)deadzone;

    // Prevent divide-by-zero and underflow
    if (max_value <= effective_deadzone) {
        // Fallback: unity scaling in Q8
        return (1u << 8);
    }

    uint32_t effective_range = (uint32_t)max_value - effective_deadzone;

    // Precompute Q8 scaler: (target_range / effective_range) in Q8
    return ((uint32_t)target_range << 8) / effective_range;
}

// Fast real-time scaling using the pre-calculated scaler
// 'deadzone' here is the fixed deadzone (same param as above)
uint16_t _hover_scale_input(uint16_t input,
                            uint16_t min_value,
                            uint16_t deadzone,
                            uint32_t scaler)
{
    uint32_t effective_deadzone = (uint32_t)min_value + (uint32_t)deadzone;

    if (input <= effective_deadzone)
        return 0;

    uint32_t adjusted = (uint32_t)input - effective_deadzone;
    uint32_t scaled   = (adjusted * scaler) >> 8;   // Q8

    if (scaled > 4095)
        scaled = 4095;

    return (uint16_t)scaled;
}

void hover_calibrate_stop(void) {
    hover_config->hover_calibration_set = 1;
    _calibration_ch_active = 0x00;
}

void _hover_set_ch_to_default(uint8_t ch)
{
    hoverSlot_s *cfg = &hover_config->config[ch];
    cfg->invert = 0;
    cfg->max = 0xFFF;
    cfg->min = 0x000;
}

void hover_calibrate_start(uint8_t ch)
{
    hoverSlot_s *cfg;
    mapper_input_s tmp;

    hover_config->hover_calibration_set = 0;

    // Perform a reading
    cb_hoja_read_input(&tmp);
    
    const uint16_t half = (0xFFF>>1);

    // 0xFF indicates calibrate ALL channels
    if(ch==0xFF)
    {
        // Loop and check which inputs we should invert based on the resting analog input value
        // Reset all calibrations
        for(int i = 0; i < _used_hover_slots; i++)
        {
            uint8_t slot = _hover_slot_reader_idx[i];
            cfg = &hover_config->config[slot];

            // Reset values
            cfg->invert = 0;
            cfg->max = 0x000;
            cfg->min = 0xFFF;

            if(tmp.inputs[slot] >= half)
            {
                cfg->invert = 1;
            }
        }
        _calibration_ch_active = 0xFF;
    }
    // Otherwise calibrate a single channel
    else 
    {
        cfg = &hover_config->config[ch];

        // Reset values
        cfg->invert = 0;
        cfg->max = 0x000;
        cfg->min = 0xFFF;

        if(tmp.inputs[ch] >= half)
        {
            cfg->invert = 1;
        }
        // Channel active is ch+1
        _calibration_ch_active = ch + 1;
    }
}

void hover_init(void)
{
    // Check and default
    if(hover_config->hover_config_version != CFG_BLOCK_HOVER_VERSION)
    {
        hover_config->hover_calibration_set = 0;

        if(_used_hover_slots==0) hover_config->hover_calibration_set=1;

        hover_config->hover_config_version = CFG_BLOCK_HOVER_VERSION;
        
        for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
        {
            _hover_set_ch_to_default(i);
        }

        // Enable calibration all channels
        if(!hover_config->hover_calibration_set)
            hover_calibrate_start(0xFF);
    }

    _used_hover_slots = 0;

    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        if(input_static.input_info[i].input_type == MAPPER_INPUT_TYPE_HOVER)
        {
            hoverSlot_s *cfg = &hover_config->config[i];

            _hover_slot_reader_idx[_used_hover_slots] = i;
            _used_hover_slots++;

            _hover_scalers[i] = _hover_calculate_scaler(
                    cfg->min, 
                    cfg->max, 
                    HOVER_DEFAULT_DEADZONE,
                    0xFFF
            );
        }
    }

    static bool boot = false;

    if(!boot)
    {
        // Perform boot input capture
        cb_hoja_read_input(&_boot_capture);
        boot=true;
    }    

    // Adjust hovers to digital states
    // Perform scaling on hover channels
    for(int i = 0; i < _used_hover_slots; i++)
    {
        uint8_t slot = _hover_slot_reader_idx[i];
        const uint16_t half = (0xFFF>>1);

        _boot_capture.presses[slot] = (_boot_capture.inputs[slot] > half);
    }
}

void hover_access_safe(mapper_input_s *out)
{
    snapshot_hover_read(&_hover_snap, out);
}

mapper_input_s hover_access_boot()
{
    return _boot_capture;
}

void hover_config_command(uint8_t cmd, webreport_cmd_confirm_t cb)
{
    uint8_t calibrate_ch = cmd & 0x3F;
    uint8_t instruction = (cmd >> 6);

    switch(instruction)
    {
        default:
        case 0:
        // Reload/Calibrate Stop
        hover_calibrate_stop();
        hover_init();
        break;

        // Calibrate
        case 1:
        hover_calibrate_start( (calibrate_ch==0x3F) ? 0xFF : calibrate_ch );
        break;
    }

    cb(CFG_BLOCK_HOVER, cmd, true, NULL, 0);
}

void hover_task(uint64_t timestamp)
{
    mapper_input_s input = {0};
    
    // Perform a reading
    cb_hoja_read_input(&input);

    // Apply inversion FIRST, before calibration or scaling
    for(int i = 0; i < _used_hover_slots; i++) {
        uint8_t slot = _hover_slot_reader_idx[i];
        hoverSlot_s *cfg = &hover_config->config[slot];
        
        if(cfg->invert) {
            input.inputs[slot] = 0xFFF - input.inputs[slot];
        }
    }

    if(_calibration_ch_active == 0xFF)
    {
        hoverSlot_s *cfg;

        // Iterate through hovers
        for(int i = 0; i < _used_hover_slots; i++)
        {
            uint8_t slot = _hover_slot_reader_idx[i];
            cfg = &hover_config->config[slot];
            uint16_t val = input.inputs[slot];
            bool update = false;

            if(val > cfg->max)
            {
                cfg->max = val;
                update = true;
            }
            else if (val < cfg->min)
            {
                cfg->min = val;
                update = true;
            }

            if(update)
            {
                _hover_scalers[slot] = _hover_calculate_scaler(
                    cfg->min, 
                    cfg->max, 
                    HOVER_DEFAULT_DEADZONE,
                    0xFFF
                );
            }
        }
    }
    else if(_calibration_ch_active)
    {
        hoverSlot_s *cfg;
        uint8_t target_ch = _calibration_ch_active-1;
        cfg = &hover_config->config[target_ch];
        uint16_t val = input.inputs[target_ch];

        bool update = false;

        if(val > cfg->max)
        {
            cfg->max = val;
            update = true;
        }
        else if (val < cfg->min)
        {
            cfg->min = val;
            update = true;
        }

        if(update)
        {
            _hover_scalers[target_ch] = _hover_calculate_scaler(
                cfg->min, 
                cfg->max, 
                HOVER_DEFAULT_DEADZONE,
                0xFFF
            );
        }
    }

    // Perform scaling on hover channels
    for(int i = 0; i < _used_hover_slots; i++)
    {
        uint8_t slot = _hover_slot_reader_idx[i];
        hoverSlot_s *cfg = &hover_config->config[slot];

        input.inputs[slot] = _hover_scale_input(
            input.inputs[slot], 
            cfg->min, 
            HOVER_DEFAULT_DEADZONE, 
            _hover_scalers[slot]
        );
    }

    snapshot_hover_write(&_hover_snap, &input);
}