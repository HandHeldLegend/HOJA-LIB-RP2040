#include "input/analog.h"
#include "input/stick_scaling.h"
#include "input/snapback.h"

#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#include "hal/mutex_hal.h"

#include "utilities/interval.h"

#include "switch/switch_analog.h"

// Include all ADC drivers
#include "hal/adc_hal.h"
#include "drivers/adc/mcp3002.h"

analog_config_u _analog_config = {0};


MUTEX_HAL_INIT(_analog_mutex);
void _analog_blocking_enter()
{
    MUTEX_HAL_ENTER_BLOCKING(&_analog_mutex);
}

bool _analog_try_enter()
{
    if(MUTEX_HAL_ENTER_TIMEOUT_US(&_analog_mutex, 10))
    {
        return true;
    }
    return false;
}

void _analog_exit()
{
    MUTEX_HAL_EXIT(&_analog_mutex);
}

analog_data_s _raw_analog_data      = {0};  // Stage 0
analog_data_s _scaled_analog_data   = {0};  // Stage 1
analog_data_s _snapback_analog_data = {0};  // Stage 2
analog_data_s _deadzone_analog_data = {0};  // Stage 3

// Access analog input data safely
void analog_access_blocking(analog_data_s *out, analog_access_t type)
{
    _analog_blocking_enter();
    switch(type)
    {
        case ANALOG_ACCESS_RAW_DATA:
        memcpy(out, &_raw_analog_data, sizeof(analog_data_s));
        break; 

        case ANALOG_ACCESS_SCALED_DATA:
        memcpy(out, &_scaled_analog_data, sizeof(analog_data_s));
        break; 

        case ANALOG_ACCESS_SNAPBACK_DATA:
        memcpy(out, &_snapback_analog_data, sizeof(analog_data_s));
        break;

        case ANALOG_ACCESS_DEADZONE_DATA:
        memcpy(out, &_deadzone_analog_data, sizeof(analog_data_s));
        break;
    }
    _analog_exit();
}

bool analog_access_try(analog_data_s *out, analog_access_t type)
{
    if(_analog_try_enter())
    {
        switch(type)
        {
            case ANALOG_ACCESS_RAW_DATA:
            memcpy(out, &_raw_analog_data, sizeof(analog_data_s));
            break; 

            case ANALOG_ACCESS_SCALED_DATA:
            memcpy(out, &_scaled_analog_data, sizeof(analog_data_s));
            break; 

            case ANALOG_ACCESS_SNAPBACK_DATA:
            memcpy(out, &_snapback_analog_data, sizeof(analog_data_s));
            break;

            case ANALOG_ACCESS_DEADZONE_DATA:
            memcpy(out, &_deadzone_analog_data, sizeof(analog_data_s));
            break;
        }
        _analog_exit();
        return true;
    }
    return false;
}

#define CENTER 2048
#define DEADZONE_DEFAULT 100

uint16_t _analog_deadzone_l = DEADZONE_DEFAULT;
uint16_t _analog_deadzone_r = DEADZONE_DEFAULT;

bool _analog_calibrate = false;
bool _analog_centered = false;
bool _analog_all_angles_got = false;
bool _analog_capture_angle = false;

void analog_init()
{
    #if defined(HOJA_ADC_CHAN_LX_INIT)
        HOJA_ADC_CHAN_LX_INIT();
    #endif

    #if defined(HOJA_ADC_CHAN_LY_INIT)
        HOJA_ADC_CHAN_LY_INIT();
    #endif

    #if defined(HOJA_ADC_CHAN_RX_INIT)
        HOJA_ADC_CHAN_RX_INIT();
    #endif

    #if defined(HOJA_ADC_CHAN_RY_INIT)
        HOJA_ADC_CHAN_RY_INIT();
    #endif

    #if defined(HOJA_ADC_CHAN_LT_INIT)
        HOJA_ADC_CHAN_LT_INIT();
    #endif

    #if defined(HOJA_ADC_CHAN_RT_INIT)
        HOJA_ADC_CHAN_RT_INIT();
    #endif

    //stick_scaling_get_settings();
    //stick_scaling_init();
    switch_analog_calibration_init();

    //if (_buttons->button_minus && _buttons->button_plus)
    //{
    //    analog_calibrate_start();
    //}
}

#define STICK_INTERNAL_CENTER 2048

#if (ADC_SMOOTHING_ENABLED==1)
#define ADC_SMOOTHING_BUFFER_SIZE ADC_SMOOTHING_STRENGTH

typedef struct {
    float buffer[ADC_SMOOTHING_BUFFER_SIZE];
    int index;
    float sum;
} RollingAverage;

void initRollingAverage(RollingAverage* ra) {
    for (int i = 0; i < ADC_SMOOTHING_BUFFER_SIZE; ++i) {
        ra->buffer[i] = 0.0f;
    }
    ra->index = 0;
    ra->sum = 0.0f;
}

void addSample(RollingAverage* ra, float sample) {
    // Subtract the value being replaced from the sum
    ra->sum -= ra->buffer[ra->index];
    
    // Add the new sample to the buffer and sum
    ra->buffer[ra->index] = sample;
    ra->sum += sample;
    
    // Move to the next index, wrapping around if necessary
    ra->index = (ra->index + 1) % ADC_SMOOTHING_BUFFER_SIZE;
}

float getAverage(RollingAverage* ra) {
    return ra->sum / ADC_SMOOTHING_BUFFER_SIZE;
}
#endif

void analog_config_cmd(analog_cmd_t cmd, const uint8_t *data, setting_callback_t cb)
{
    const uint8_t cb_dat[2] = {cmd, 1};

    switch(cmd)
    {
        default:
        if(cb!=NULL)
            cb(cb_dat, 0); // Do nothing
        return;
        break;

        case ANALOG_CMD_SET_CENTERS:
        break;

        case ANALOG_CMD_CALIBRATE_START:
        break;

        case ANALOG_CMD_CALIBRATE_STOP:
        break;

        case ANALOG_CMD_CAPTURE_ANGLE:
        break;

        case ANALOG_CMD_UPDATE_ANGLE:
        break;

        case ANALOG_CMD_SET_DEADZONES:
        break;

        case ANALOG_CMD_SET_SNAPBACK:
        break;
    }
}

// Read analog values
void _analog_read_raw()
{
    uint16_t lx = STICK_INTERNAL_CENTER;
    uint16_t ly = STICK_INTERNAL_CENTER;
    uint16_t rx = STICK_INTERNAL_CENTER;
    uint16_t ry = STICK_INTERNAL_CENTER;

    #if (ADC_SMOOTHING_ENABLED==1)
    static RollingAverage ralx = {0};
    static RollingAverage raly = {0};
    static RollingAverage rarx = {0};
    static RollingAverage rary = {0};
    #endif

    #ifndef HOJA_ADC_CHAN_LX_READ
        #warning "HOJA_ADC_CHAN_LX_READ undefined and won't produce input."
    #else
        lx = HOJA_ADC_CHAN_LX_READ();
    #endif 

    #ifndef HOJA_ADC_CHAN_LY_READ
        #warning "HOJA_ADC_CHAN_LY_READ undefined and won't produce input."
    #else
        ly = HOJA_ADC_CHAN_LY_READ();
    #endif 

    #ifndef HOJA_ADC_CHAN_RX_READ
        #warning "HOJA_ADC_CHAN_RX_READ undefined and won't produce input."
    #else
        rx = HOJA_ADC_CHAN_RX_READ();
    #endif 

    #ifndef HOJA_ADC_CHAN_RY_READ
        #warning "HOJA_ADC_CHAN_RY_READ undefined and won't produce input."
    #else
        ry = HOJA_ADC_CHAN_RY_READ();
    #endif 

    #if (ADC_SMOOTHING_ENABLED==1)
        addSample(&ralx, lx);
        addSample(&raly, ly);
        addSample(&rarx, rx);
        addSample(&rary, ry);

        // Convert data
        _raw_analog_data.lx = (uint16_t) getAverage(&ralx);
        _raw_analog_data.ly = (uint16_t) getAverage(&raly);
        _raw_analog_data.rx = (uint16_t) getAverage(&rarx);
        _raw_analog_data.ry = (uint16_t) getAverage(&rary);
    #else
        _raw_analog_data.lx = lx;
        _raw_analog_data.ly = ly;
        _raw_analog_data.rx = rx;
        _raw_analog_data.ry = ry;
    #endif
}

const uint32_t _analog_interval = 200;

void analog_task(uint32_t timestamp)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, _analog_interval, &interval))
    {
        _analog_blocking_enter();

        // Read raw analog sticks
        _analog_read_raw();

        // Temp debug
        memcpy(&_snapback_analog_data, &_raw_analog_data, sizeof(analog_data_s));
        memcpy(&_deadzone_analog_data, &_raw_analog_data, sizeof(analog_data_s));

        _analog_exit();
    }
}
