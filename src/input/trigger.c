#include "input/trigger.h"
#include <stddef.h>
#include <string.h>
#include "hoja.h"

#include "settings_shared_types.h"
#include "utilities/settings.h"
#include "utilities/interval.h"
#include "utilities/crosscore_snapshot.h"

#include "devices/adc.h"
#include "board_config.h"

#include "hal/mutex_hal.h"

trigger_data_s  _safe_raw_triggers       = {0};
trigger_data_s  _safe_scaled_triggers    = {0};

trigger_data_s  _raw_triggers       = {0};
trigger_data_s  _scaled_triggers    = {0};

uint32_t _lt_scaler = 0;
uint32_t _rt_scaler = 0;
uint16_t _lt_deadzone = 0;
uint16_t _rt_deadzone = 0;
bool _trigger_calibration = false;

SNAPSHOT_TYPE(trigger, trigger_data_s);
snapshot_trigger_t _trigger_raw_snap;
snapshot_trigger_t _trigger_scaled_snap;

#define MAX_ANALOG_OUT 4095

// Access button data with fallback
void trigger_access_safe(trigger_data_s *out, trigger_access_t type)
{
    switch(type)
    {
        case TRIGGER_ACCESS_RAW_DATA:
        snapshot_trigger_read(&_trigger_raw_snap, out);
        break;

        case TRIGGER_ACCESS_SCALED_DATA:
        snapshot_trigger_read(&_trigger_scaled_snap, out);
        break;
    }
}


bool _trigger_calibrate = false;
void _trigger_calibrate_stop()
{
    hoja_set_notification_status(COLOR_BLACK);
    _trigger_calibrate = false;
}

void _trigger_calibrate_start()
{
    hoja_set_notification_status(COLOR_YELLOW);
    // Reset analog distances
    trigger_config->left_max = 0;
    trigger_config->left_min = MAX_ANALOG_OUT;

    trigger_config->right_max = 0;
    trigger_config->right_min = MAX_ANALOG_OUT;
    _trigger_calibrate = true;
}

void _trigger_calibrate_task()
{
    if(_raw_triggers.left_analog < trigger_config->left_min)
        trigger_config->left_min = _raw_triggers.left_analog;

    if(_raw_triggers.right_analog < trigger_config->right_min)
        trigger_config->right_min = _raw_triggers.right_analog;

    if(_raw_triggers.left_analog > trigger_config->left_max)
        trigger_config->left_max = _raw_triggers.left_analog;

    if(_raw_triggers.right_analog > trigger_config->right_max)
        trigger_config->right_max = _raw_triggers.right_analog;
}

// Calculate scaler when deadzone or range changes
// Returns 16.16 fixed point scaler
uint32_t _trigger_calculate_scaler(uint16_t deadzone, uint16_t max_value, uint16_t target_range) {
    uint32_t effective_range = max_value - deadzone;
    return ((uint32_t)target_range << 8) / effective_range;  // One-time expensive divide
}

// Fast real-time scaling using the pre-calculated scaler
uint16_t _trigger_scale_input(uint16_t input, uint16_t deadzone, uint32_t scaler) {
    if (input < deadzone) 
        return 0;

    uint32_t adjusted = (uint32_t)input - deadzone;
    uint32_t scaled   = (adjusted * scaler) >> 8;

    if (scaled > MAX_ANALOG_OUT) 
        scaled = MAX_ANALOG_OUT;

    return (uint16_t)scaled;
}

void trigger_task(uint64_t timestamp)
{
    #if defined(HOJA_ADC_RT_CFG) || defined(HOJA_ADC_LT_CFG)
    static interval_s interval = {0};
    if(interval_run(timestamp, 1000, &interval))
    {
        #if defined(HOJA_ADC_RT_CFG)
        if(trigger_config->right_disabled)
            _raw_triggers.right_analog = 0;
        else 
            _raw_triggers.right_analog = adc_read_rt();
        #endif

        #if defined(HOJA_ADC_LT_CFG)
        if(trigger_config->left_disabled)
            _raw_triggers.left_analog = 0;
        else 
            _raw_triggers.left_analog = adc_read_lt();
        #endif

        snapshot_trigger_write(&_trigger_raw_snap, &_raw_triggers);

        if(_trigger_calibrate)
        {
            _trigger_calibrate_task();
            _scaled_triggers.right_analog = 0;
            _scaled_triggers.left_analog  = 0;
        }
        else 
        {
            _scaled_triggers.right_analog = _trigger_scale_input(
                _raw_triggers.right_analog, 
                _rt_deadzone, _rt_scaler);

            _scaled_triggers.left_analog = _trigger_scale_input(
                _raw_triggers.left_analog, 
                _lt_deadzone, _lt_scaler);

            snapshot_trigger_write(&_trigger_scaled_snap, &_scaled_triggers);
        }
    }
    #endif
}

void trigger_config_cmd(trigger_cmd_t cmd, webreport_cmd_confirm_t  cb)
{
    bool do_cb = false;

    switch(cmd)
    {
        default:
        break;

        case TRIGGER_CMD_REFRESH:
        break;

        case TRIGGER_CMD_CALIBRATE_START:
            _trigger_calibrate_start();
            do_cb = true;
        break;

        case TRIGGER_CMD_CALIBRATE_STOP:
            _trigger_calibrate_stop();
            trigger_init();
            do_cb = true;
        break;
    }

    if(do_cb)
        cb(CFG_BLOCK_TRIGGER, cmd, true, NULL, 0);
}

bool trigger_init()
{
    if(trigger_config->trigger_config_version != CFG_BLOCK_TRIGGER_VERSION)
    {
        trigger_config->trigger_config_version = CFG_BLOCK_TRIGGER_VERSION;
        trigger_config->left_deadzone   = 0;
        trigger_config->right_deadzone  = 0;
        trigger_config->left_hairpin_value  = 0;
        trigger_config->right_hairpin_value = 0;
        trigger_config->left_disabled   = 0;
        trigger_config->right_disabled  = 0;
        trigger_config->left_min    = 0;
        trigger_config->right_min   = 0;
        trigger_config->left_max    = MAX_ANALOG_OUT;
        trigger_config->right_max   = MAX_ANALOG_OUT;
        trigger_config->left_static_output_value    = MAX_ANALOG_OUT;
        trigger_config->right_static_output_value   = MAX_ANALOG_OUT;
    }

    if(trigger_config->left_hairpin_value<128) trigger_config->left_hairpin_value=128;
    if(trigger_config->right_hairpin_value<128) trigger_config->right_hairpin_value=128;

    _lt_deadzone = trigger_config->left_min + trigger_config->left_deadzone;
    _lt_scaler = _trigger_calculate_scaler(_lt_deadzone,
        trigger_config->left_max, MAX_ANALOG_OUT);

    _rt_deadzone = trigger_config->right_min + trigger_config->right_deadzone;
    _rt_scaler = _trigger_calculate_scaler(_rt_deadzone,
        trigger_config->right_max, MAX_ANALOG_OUT);

    return true;
}

