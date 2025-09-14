#include "input/analog.h"
#include "input/stick_scaling.h"
#include "input/snapback.h"
#include "input/stick_deadzone.h"
#include "input/idle_manager.h"

#include "devices/adc.h"

#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"

#include "usb/webusb.h"

#include "utilities/interval.h"

#include "switch/switch_analog.h"

#include "utilities/settings.h"
#include "utilities/crosscore_snapshot.h"

#include "board_config.h"

// Analog smoothing
#ifdef ADC_SMOOTHING_STRENGTH
  #if (ADC_SMOOTHING_STRENGTH > 1)
    #define ADC_SMOOTHING_ENABLED 1
  #else
    #define ADC_SMOOTHING_ENABLED 0
  #endif
#else
  #warning "ADC_SMOOTHING_STRENGTH isn't defined. It's optional to smooth out analog input. Typically not recommended." 
  #define ADC_SMOOTHING_ENABLED 0
#endif

#define ANALOG_MAX_DISTANCE 2048
#define ANALOG_HALF_DISTANCE 2048
#define DEADZONE_DEFAULT 100
#define CAP_ANALOG(value) ((value>ANALOG_MAX_DISTANCE) ? ANALOG_MAX_DISTANCE : (value < (-ANALOG_MAX_DISTANCE)) ? (-ANALOG_MAX_DISTANCE) : value)

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

#define STICK_INTERNAL_CENTER 2048
#define STICK_OPTIONAL_INVERT(val, invert) (invert ? 0xFFF - val : val)

SNAPSHOT_TYPE(js, analog_data_s);

snapshot_js_t _js_raw;
snapshot_js_t _js_scaled;
snapshot_js_t _js_snapback;
snapshot_js_t _js_deadzone;

analog_data_s _raw_analog_data      = {0};  // Stage 0
analog_data_s _scaled_analog_data   = {0};  // Stage 1
analog_data_s _snapback_analog_data = {0};  // Stage 2
analog_data_s _deadzone_analog_data = {0};  // Stage 3
int16_t       _center_offsets[4]    = {0};

analog_data_s _safe_raw_analog_data      = {0};  // Stage 0
analog_data_s _safe_scaled_analog_data   = {0};  // Stage 1
analog_data_s _safe_snapback_analog_data = {0};  // Stage 2
analog_data_s _safe_deadzone_analog_data = {0};  // Stage 3

// Access analog input data safely
void analog_access_safe(analog_data_s *out, analog_access_t type)
{
    switch(type)
    {
        case ANALOG_ACCESS_RAW_DATA:
        snapshot_js_read(&_js_raw, out);
        break; 

        case ANALOG_ACCESS_SCALED_DATA:
        snapshot_js_read(&_js_scaled, out);
        break; 

        case ANALOG_ACCESS_SNAPBACK_DATA:
        snapshot_js_read(&_js_snapback, out);
        break;

        case ANALOG_ACCESS_DEADZONE_DATA:
        snapshot_js_read(&_js_deadzone, out);
        break;
    }
}

void analog_init()
{
    adc_devices_init();

    switch_analog_calibration_init();
    stick_scaling_init();

    _center_offsets[0] = (int16_t) STICK_INTERNAL_CENTER - analog_config->lx_center;
    _center_offsets[1] = (int16_t) STICK_INTERNAL_CENTER - analog_config->ly_center;
    _center_offsets[2] = (int16_t) STICK_INTERNAL_CENTER - analog_config->rx_center;
    _center_offsets[3] = (int16_t) STICK_INTERNAL_CENTER - analog_config->ry_center;
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

    lx = adc_read_lx();
    ly = adc_read_ly();
    rx = adc_read_rx();
    ry = adc_read_ry();

    #if (ADC_SMOOTHING_ENABLED==1)
        addSample(&ralx, lx);
        addSample(&raly, ly);
        addSample(&rarx, rx);
        addSample(&rary, ry);

        // Convert data
        _raw_analog_data.lx = STICK_OPTIONAL_INVERT((uint16_t) getAverage(&ralx), analog_config->lx_invert);
        _raw_analog_data.ly = STICK_OPTIONAL_INVERT((uint16_t) getAverage(&raly), analog_config->ly_invert);
        _raw_analog_data.rx = STICK_OPTIONAL_INVERT((uint16_t) getAverage(&rarx), analog_config->rx_invert);
        _raw_analog_data.ry = STICK_OPTIONAL_INVERT((uint16_t) getAverage(&rary), analog_config->ry_invert);
    #else
        _raw_analog_data.lx = STICK_OPTIONAL_INVERT(lx, analog_config->lx_invert);
        _raw_analog_data.ly = STICK_OPTIONAL_INVERT(ly, analog_config->ly_invert);
        _raw_analog_data.rx = STICK_OPTIONAL_INVERT(rx, analog_config->rx_invert);
        _raw_analog_data.ry = STICK_OPTIONAL_INVERT(ry, analog_config->ry_invert);
    #endif
}

volatile bool _capture_centers = false;
void _capture_center_offsets()
{
    int32_t lx_sum = 0, ly_sum = 0, rx_sum = 0, ry_sum = 0;
    
    // Discard the first reading
    _analog_read_raw();
    
    // Average over 8 readings
    for (int i = 0; i < 8; i++) {
        _analog_read_raw();
        lx_sum += _raw_analog_data.lx & 0x7FFF;
        ly_sum += _raw_analog_data.ly & 0x7FFF;
        rx_sum += _raw_analog_data.rx & 0x7FFF;
        ry_sum += _raw_analog_data.ry & 0x7FFF;
        sys_hal_sleep_ms(1);
    }
    
    analog_config->lx_center = (lx_sum / 8);
    analog_config->ly_center = (ly_sum / 8);
    analog_config->rx_center = (rx_sum / 8);
    analog_config->ry_center = (ry_sum / 8);

    _center_offsets[0] = (int16_t) STICK_INTERNAL_CENTER - analog_config->lx_center;
    _center_offsets[1] = (int16_t) STICK_INTERNAL_CENTER - analog_config->ly_center;
    _center_offsets[2] = (int16_t) STICK_INTERNAL_CENTER - analog_config->rx_center;
    _center_offsets[3] = (int16_t) STICK_INTERNAL_CENTER - analog_config->ry_center;
}

void _capture_input_angle(float *left, float *right)
{
    // Extract angles from raw analog data and store them
    *left   = stick_scaling_coordinates_to_angle(_raw_analog_data.lx, _raw_analog_data.ly);
    *right  = stick_scaling_coordinates_to_angle(_raw_analog_data.rx, _raw_analog_data.ry);
}

// Helper function to convert angle/distance to coordinate pair
void analog_angle_distance_to_coordinate(float angle, float distance, int16_t *out) 
{    
    // Normalize angle to 0-360 range
    angle = fmodf(angle, 360.0f);
    if (angle < 0) angle += 360.0f;

    angle = roundf(angle);
    
    // Convert angle to radians
    float angle_radians = angle * M_PI / 180.0f;
    
    // Calculate X and Y coordinates
    // Limit to -2048 to +2048 range
    out[0] = (int)(distance * cosf(angle_radians));
    out[1] = (int)(distance * sinf(angle_radians));
    
    // Clamp to prevent exceeding the specified range
    out[0] = fmaxf(-2048, fminf(2048, out[0]));
    out[1] = fmaxf(-2048, fminf(2048, out[1]));
}

void analog_config_command(analog_cmd_t cmd, webreport_cmd_confirm_t cb)
{
    float left = 0.0f;
    float right = 0.0f;
    bool do_cb = false;

    switch(cmd)
    {
        default:
        cb(CFG_BLOCK_ANALOG, cmd, false, NULL, 0);
        break;

        case ANALOG_CMD_CALIBRATE_START:
            _capture_centers = true;
            stick_scaling_calibrate_start(true);
            do_cb = true;
        break;

        case ANALOG_CMD_CALIBRATE_STOP:
            stick_scaling_calibrate_start(false);
            _capture_centers = false;
            do_cb = true;
        break;

        case ANALOG_CMD_CAPTURE_ANGLE:
            // Capture angles
            _capture_input_angle(&left, &right);
            uint8_t buffer[8] = {0};
            memcpy(buffer, &left, sizeof(float));
            memcpy(buffer+4, &right, sizeof(float));
            cb(CFG_BLOCK_ANALOG, cmd, true, buffer, 8);
            return;
        break;
    }

    if(do_cb)
        cb(CFG_BLOCK_ANALOG, cmd, true, NULL, 0);
}

bool _is_analog_dist_changed(analog_data_s *current)
{
    static uint16_t dist_l = 0;
    static uint16_t dist_r = 0;
    bool return_value = false;

    if(dist_l != current->ldistance) return_value = true;
    if(dist_r != current->rdistance) return_value = true;

    dist_l = current->ldistance;
    dist_r = current->rdistance;

    return return_value;
}

// 2Khz analog task

#define DEBUG_ANALOG_OUTPUT 1

void analog_task(uint64_t timestamp)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, ANALOG_POLL_INTERVAL, &interval))
    {
        // Read raw analog sticks
        _analog_read_raw();

        if(_capture_centers)
        {
            _capture_center_offsets();
            _capture_centers = false;
        }

        // Offset data by center offsets
        _raw_analog_data.lx += _center_offsets[0];
        _raw_analog_data.ly += _center_offsets[1];
        _raw_analog_data.rx += _center_offsets[2];
        _raw_analog_data.ry += _center_offsets[3];

        // Rebase data to 0 center
        _raw_analog_data.lx -= ANALOG_HALF_DISTANCE;
        _raw_analog_data.ly -= ANALOG_HALF_DISTANCE;
        _raw_analog_data.rx -= ANALOG_HALF_DISTANCE;
        _raw_analog_data.ry -= ANALOG_HALF_DISTANCE;
        snapshot_js_write(&_js_raw, &_raw_analog_data);

        stick_scaling_process(&_raw_analog_data, &_scaled_analog_data);
        snapshot_js_write(&_js_scaled, &_scaled_analog_data);

        snapback_process(&_scaled_analog_data, &_snapback_analog_data);
        snapshot_js_write(&_js_snapback, &_snapback_analog_data);

        stick_deadzone_process(&_snapback_analog_data, &_deadzone_analog_data);

        if(_is_analog_dist_changed(&_deadzone_analog_data)) idle_manager_heartbeat();

        // Cap values
        _deadzone_analog_data.lx = CAP_ANALOG(_deadzone_analog_data.lx);
        _deadzone_analog_data.ly = CAP_ANALOG(_deadzone_analog_data.ly);
        _deadzone_analog_data.rx = CAP_ANALOG(_deadzone_analog_data.rx);
        _deadzone_analog_data.ry = CAP_ANALOG(_deadzone_analog_data.ry);
        
        snapshot_js_write(&_js_deadzone, &_deadzone_analog_data);
    }
}
