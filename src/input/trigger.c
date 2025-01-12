#include "input/trigger.h"
#include <stddef.h>
#include <string.h>

#include "settings_shared_types.h"
#include "utilities/settings.h"
#include "utilities/interval.h"

#include "hal/mutex_hal.h"

#include "hal/adc_hal.h"
#include "drivers/adc/mcp3002.h"

trigger_data_s  _safe_raw_triggers       = {0};
trigger_data_s  _safe_scaled_triggers    = {0};

trigger_data_s  _raw_triggers       = {0};
trigger_data_s  _scaled_triggers    = {0};

uint32_t _lt_scaler = 0;
uint32_t _rt_scaler = 0;
uint16_t _lt_deadzone = 0;
uint16_t _rt_deadzone = 0;
bool _trigger_calibration = false;

#define MAX_ANALOG_OUT 4095

MUTEX_HAL_INIT(_trigger_mutex);
uint32_t _trigger_mutex_owner = 0;

bool _trigger_try_enter()
{
    if(MUTEX_HAL_ENTER_TRY(&_trigger_mutex, &_trigger_mutex_owner))
    {
        return true;
    }
    return false;
}

void _trigger_exit()
{
    MUTEX_HAL_EXIT(&_trigger_mutex);
}

void trigger_gc_process(button_data_s *b_state, trigger_data_s *t_state)
{
    switch(trigger_config->trigger_mode_gamecube)
    {
        default:
        case GC_SP_MODE_NONE:
        break;

        case GC_SP_MODE_LT:
            t_state->left_analog = 0;

            t_state->left_analog |= (b_state->trigger_l) ? (trigger_config->left_static_output_value) : 0;
            t_state->left_analog |= (b_state->trigger_zl) ? 255 : 0;
        break;

        case GC_SP_MODE_RT:
            t_state->right_analog = 0;

            t_state->right_analog |= (b_state->trigger_l) ? (trigger_config->right_static_output_value) : 0;
            t_state->right_analog |= (b_state->trigger_zr) ? 255 : 0;
        break;

        case GC_SP_MODE_DUALZ:
            if(b_state->trigger_l)
                b_state->trigger_r |= 1;
        break;
    }
}

// Access button data with fallback
void trigger_access_safe(trigger_data_s *out, trigger_access_t type)
{
    if(_trigger_try_enter())
    {
        switch(type)
        {
            case TRIGGER_ACCESS_RAW_DATA:
            memcpy(out, &_safe_raw_triggers, sizeof(trigger_data_s));
            break;

            case TRIGGER_ACCESS_SCALED_DATA:
            memcpy(out, &_safe_scaled_triggers, sizeof(trigger_data_s));
            break;
        }
        _trigger_exit();
    }
}

void trigger_task(uint32_t timestamp)
{
    #if defined(HOJA_ADC_CHAN_RT_READ) || defined(HOJA_ADC_CHAN_LT_READ)
    static interval_s interval = {0};
    if(interval_run(timestamp, 1000, &interval))
    {
        #if defined(HOJA_ADC_CHAN_RT_READ)
            _raw_triggers.right_analog = HOJA_ADC_CHAN_RT_READ();
            _scaled_triggers.right_analog = _raw_triggers.right_analog;
        #endif

        #if defined(HOJA_ADC_CHAN_LT_READ)
            _raw_triggers.left_analog = HOJA_ADC_CHAN_LT_READ();
            _scaled_triggers.left_analog = _raw_triggers.left_analog;
        #endif
    }

    if(_trigger_try_enter())
    {
        memcpy(&_safe_raw_triggers, &_raw_triggers, sizeof(trigger_data_s));
        memcpy(&_safe_scaled_triggers, &_scaled_triggers, sizeof(trigger_data_s));
        _trigger_exit();
    }

    #endif
}

// Calculate scaler when deadzone or range changes
// Returns 16.16 fixed point scaler
uint32_t _trigger_calculate_scaler(uint16_t deadzone, uint16_t max_value, uint16_t target_range) {
    uint32_t effective_range = max_value - deadzone;
    return ((uint32_t)target_range << 8) / effective_range;  // One-time expensive divide
}

// Fast real-time scaling using the pre-calculated scaler
int16_t _trigger_scale_input(int16_t input, uint16_t deadzone, uint32_t scaler) {
    if (input <= deadzone) return 0;

    uint32_t adjusted = (uint32_t) input - deadzone;
    return (int16_t) ((adjusted * scaler) >> 8);  // Just multiply and shift
}

void trigger_config_cmd(trigger_cmd_t cmd, command_confirm_t cb)
{
    bool do_cb = false;

    switch(cmd)
    {
        default:
        break;

        case TRIGGER_CMD_REFRESH:
        break;

        case TRIGGER_CMD_CALIBRATE_START:
            do_cb = true;
        break;

        case TRIGGER_CMD_CALIBRATE_STOP:
            do_cb = true;
        break;
    }

    if(do_cb)
        cb(CFG_BLOCK_TRIGGER, cmd, NULL, 0);
}

bool trigger_init()
{
    _lt_deadzone = trigger_config->left_min + trigger_config->left_deadzone;
    _lt_scaler = _trigger_calculate_scaler(_lt_deadzone,
        trigger_config->left_max, MAX_ANALOG_OUT);

    _rt_deadzone = trigger_config->right_min + trigger_config->right_deadzone;
    _rt_scaler = _trigger_calculate_scaler(_rt_deadzone,
        trigger_config->right_max, MAX_ANALOG_OUT);

    #if defined(HOJA_ADC_CHAN_RT_INIT)
        HOJA_ADC_CHAN_RT_INIT();
    #endif 

    #if defined(HOJA_AD_CHAN_LT_INIT)
        HOJA_AD_CHAN_LT_INIT();
    #endif
}

