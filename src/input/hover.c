#include "hoja.h"
#include "board_config.h"
#include "input/hover.h"
#include "utilities/settings.h"
#include "utilities/crosscore_utils.h"
#include "input_shared_types.h"
#include "input/macros/macro_tourney.h"

#include <string.h>

SNAPSHOT_TYPE(hover, mapper_input_s);

snapshot_hover_t _hover_snap;

uint8_t _hover_slot_reader_idx[36] = {0};
uint32_t _hover_scalers[36] = {0};
uint8_t _used_hover_slots = 0;
uint8_t _calibration_ch_active = 0x00;

#if defined(HOJA_HOVER_INVERTS)
uint8_t _hover_inverts[36] = HOJA_HOVER_INVERTS;
#else 
uint8_t _hover_inverts[36] = {0};
#endif

mapper_input_s _boot_capture = {0};

#define HOVER_DEFAULT_DEADZONE 128

#if defined(HOJA_MECHANICAL_TRIGGERS) && (HOJA_MECHANICAL_TRIGGERS > 0)
// Auto detection judges each click by how far the trigger's analog reading moved since the last
// release (raw counts, after inversion). A stock trigger clicks only at the end of its travel; a
// mechanical one barely moves the sensor, if it reaches it at all.
#define HOVER_MECH_TRAVEL_MAX           300  // A click after less travel than this had none
#define HOVER_MECH_ANALOG_TRAVEL_MIN    1200 // A click after more than this ended real travel
// Clicks in a row before the finding changes, so one odd reading never takes the analog away
#define HOVER_MECH_CLICKS_TO_SET        5
#define HOVER_MECH_CLICKS_TO_CLEAR      2

typedef struct
{
    uint16_t low;   // Lowest and highest reading since the click was last released
    uint16_t high;
    uint8_t  mech_clicks;
    uint8_t  analog_clicks;
    bool     armed; // Seen released since boot, so low and high mean something
    bool     clicked;
} hover_mech_s;

static hover_mech_s _hover_mech[2];
static const uint8_t _hover_mech_click[2]  = {INPUT_CODE_LT, INPUT_CODE_RT};
static const uint8_t _hover_mech_analog[2] = {INPUT_CODE_LT_ANALOG, INPUT_CODE_RT_ANALOG};

static void _hover_mech_init(void)
{
    // Settings from older firmware have 0xFF here: both triggers start on Auto
    if (hover_config->trigger_hw_version != TRIGGER_HW_VERSION)
    {
        hover_config->trigger_hw[0] = TRIGGER_HW_AUTO;
        hover_config->trigger_hw[1] = TRIGGER_HW_AUTO;
        hover_config->trigger_hw_version = TRIGGER_HW_VERSION;
    }

    memset(_hover_mech, 0, sizeof(_hover_mech));
}

// One reading of a trigger. True when the finding changed (and wants saving).
static bool _hover_mech_track(uint8_t side, uint16_t raw, bool click)
{
    hover_mech_s *m = &_hover_mech[side];
    uint8_t *hw = &hover_config->trigger_hw[side];

    if (!click)
    {
        if (!m->armed || m->clicked)
        {
            m->low = raw;
            m->high = raw;
            m->armed = true;
        }
        else
        {
            if (raw < m->low) m->low = raw;
            if (raw > m->high) m->high = raw;
        }
        m->clicked = false;
        return false;
    }

    // Judge the click once, as it closes
    if (m->clicked || !m->armed)
    {
        m->clicked = true;
        return false;
    }
    m->clicked = true;

    uint16_t top = (raw > m->high) ? raw : m->high;
    uint16_t travel = (top > m->low) ? (top - m->low) : 0;
    bool detected = (*hw & TRIGGER_HW_DETECTED_MECHANICAL) != 0;
    bool changed = false;

    if (travel < HOVER_MECH_TRAVEL_MAX)
    {
        m->analog_clicks = 0;
        if (!detected && ++m->mech_clicks >= HOVER_MECH_CLICKS_TO_SET)
        {
            *hw |= TRIGGER_HW_DETECTED_MECHANICAL;
            changed = true;
        }
    }
    else if (travel > HOVER_MECH_ANALOG_TRAVEL_MIN)
    {
        // An analog trigger put back in
        m->mech_clicks = 0;
        if (detected && ++m->analog_clicks >= HOVER_MECH_CLICKS_TO_CLEAR)
        {
            *hw &= (uint8_t)~TRIGGER_HW_DETECTED_MECHANICAL;
            changed = true;
        }
    }
    else
    {
        m->mech_clicks = 0;
        m->analog_clicks = 0;
    }

    if (changed)
    {
        m->mech_clicks = 0;
        m->analog_clicks = 0;
    }
    return changed;
}
#endif

bool hover_trigger_mechanical(uint8_t side)
{
#if defined(HOJA_MECHANICAL_TRIGGERS) && (HOJA_MECHANICAL_TRIGGERS > 0)
    if (side > 1 || hover_config->trigger_hw_version != TRIGGER_HW_VERSION)
        return false;

    uint8_t hw = hover_config->trigger_hw[side];
    switch (hw & TRIGGER_HW_CHOICE_MASK)
    {
        case TRIGGER_HW_MECHANICAL:
            return true;

        case TRIGGER_HW_ANALOG:
            return false;

        // Auto, and choices this firmware does not know
        default:
            return (hw & TRIGGER_HW_DETECTED_MECHANICAL) != 0;
    }
#else
    (void)side;
    return false;
#endif
}

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
    
    const uint16_t half = (3000);

    // 0xFF indicates calibrate ALL channels
    if(ch==0xFF)
    {
        // Loop and check which inputs we should invert based on the resting analog input value
        // Reset all calibrations
        for(int i = 0; i < _used_hover_slots; i++)
        {
            uint8_t slot = _hover_slot_reader_idx[i];
            cfg = &hover_config->config[slot];

            cfg->max = 0x000;
            cfg->min = 0xFFF;
        }
        _calibration_ch_active = 0xFF;
    }
    // Otherwise calibrate a single channel
    else 
    {
        cfg = &hover_config->config[ch];

        cfg->max = 0x000;
        cfg->min = 0xFFF;

        _calibration_ch_active = ch + 1;
    }
}

void hover_init(void)
{
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

    // Check and default
    if(hover_config->hover_config_version != CFG_BLOCK_HOVER_VERSION)
    {
        hover_config->hover_calibration_set = 0;

        if(_used_hover_slots==0) hover_config->hover_calibration_set=1;

        hover_config->hover_config_version = CFG_BLOCK_HOVER_VERSION;

        // A reset block (the app's settings reset blanks the version byte) puts the triggers back on Auto
        hover_config->trigger_hw_version = 0xFF;

        for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
        {
            _hover_set_ch_to_default(i);
        }

        // Enable calibration all channels
        if(!hover_config->hover_calibration_set)
            hover_calibrate_start(0xFF);
    }
    // A calibrated block written over one being calibrated (the app restoring settings after an
    // update reset them) ends the calibration; otherwise it would overwrite what was written
    else if (hover_config->hover_calibration_set)
    {
        _calibration_ch_active = 0x00;
    }

#if defined(HOJA_MECHANICAL_TRIGGERS) && (HOJA_MECHANICAL_TRIGGERS > 0)
    _hover_mech_init();
#endif

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
        hoverSlot_s *cfg = &hover_config->config[slot];
        const uint16_t half = (1000);
        if(_hover_inverts[slot]) {
            _boot_capture.inputs[slot] = 0xFFF - _boot_capture.inputs[slot];
        }

        // Scale exactly as the runtime path does, so an idle channel reads 0.
        // Without this the capture kept the raw post-invert reading (measured
        // 1801 on an idle latte_pro trigger), which made every boot combo
        // referencing a hover slot read as held, and left the analog face/dpad
        // comparisons working on resting offsets rather than actual travel.
        _boot_capture.inputs[slot] = _hover_scale_input(
            _boot_capture.inputs[slot],
            cfg->min,
            HOVER_DEFAULT_DEADZONE,
            _hover_scalers[slot]
        );

        _boot_capture.presses[slot] = (_boot_capture.inputs[slot] > half);
    }

#if defined(HOJA_MECHANICAL_TRIGGERS) && (HOJA_MECHANICAL_TRIGGERS > 0)
    for (uint8_t s = 0; s < 2; s++)
    {
        if (hover_trigger_mechanical(s))
        {
            _boot_capture.inputs[_hover_mech_analog[s]] = 0;
            _boot_capture.presses[_hover_mech_analog[s]] = false;
        }
    }
#endif
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

    // Tournament mode
    if(macro_tourney_check())
    {
        const uint8_t codes[] = {INPUT_CODE_UP, INPUT_CODE_DOWN, INPUT_CODE_LEFT, INPUT_CODE_RIGHT,
                                 INPUT_CODE_HOME, INPUT_CODE_START, INPUT_CODE_SHARE, INPUT_CODE_SELECT};
        for(int i = 0; i < 8; i++)
        {
            input.inputs[codes[i]]  = 0;
            input.presses[codes[i]] = 0;
        }
    }

    // Apply inversion FIRST, before calibration or scaling
    for(int i = 0; i < _used_hover_slots; i++) {
        uint8_t slot = _hover_slot_reader_idx[i];
        hoverSlot_s *cfg = &hover_config->config[slot];
        
        if(_hover_inverts[slot]) {
            input.inputs[slot] = 0xFFF - input.inputs[slot];
        }
    }

#if defined(HOJA_MECHANICAL_TRIGGERS) && (HOJA_MECHANICAL_TRIGGERS > 0)
    // Detection runs whatever the owner chose (it counts only on Auto), so the app can point out a
    // choice the triggers no longer match after a swap
    bool mech_save = false;
    for (uint8_t s = 0; s < 2; s++)
        mech_save |= _hover_mech_track(s, input.inputs[_hover_mech_analog[s]], input.presses[_hover_mech_click[s]]);
    // At most a few times in the controller's life: when its triggers are swapped
    if (mech_save)
        settings_commit_blocks();
#endif

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

#if defined(HOJA_MECHANICAL_TRIGGERS) && (HOJA_MECHANICAL_TRIGGERS > 0)
    // A mechanical trigger's sensor has nothing to measure: whatever it reads (resting offset, noise
    // from the sticks' magnets) must not press anything
    for (uint8_t s = 0; s < 2; s++)
    {
        if (hover_trigger_mechanical(s))
            input.inputs[_hover_mech_analog[s]] = 0;
    }
#endif

    snapshot_hover_write(&_hover_snap, &input);
}