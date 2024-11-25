#include "input/analog.h"

#include <stdbool.h>
#include <stddef.h>

#include "hal/sys_hal.h"
#include "hal/mutex_hal.h"

#include "utilities/interval.h"

MUTEX_HAL_INIT(_analog_mutex);
void _analog_safe_enter()
{
    MUTEX_HAL_ENTER_BLOCKING(&_analog_mutex);
}

void _analog_exit()
{
    MUTEX_HAL_EXIT(&_analog_mutex);
}

// Access analog input data safely
void analog_access(analog_data_s *out, analog_access_t type)
{
    _input_analog_safe_enter();
    switch(type)
    {
        case ANALOG_ACCESS_RAW_DATA:
        break; 

        case ANALOG_ACCESS_SCALED_DATA:
        break; 

        case ANALOG_ACCESS_SNAPBACK_DATA:
        break;
    }
    _input_analog_exit();
}

#define CENTER 2048
#define DEADZONE_DEFAULT 100

uint16_t _analog_deadzone_l = DEADZONE_DEFAULT;
uint16_t _analog_deadzone_r = DEADZONE_DEFAULT;

const uint32_t _analog_interval = 200;

bool _analog_calibrate = false;
bool _analog_centered = false;
bool _analog_all_angles_got = false;
bool _analog_capture_angle = false;
analog_data_s *_data_in = NULL;
analog_data_s scaled_analog_data = {0};
analog_data_s *_data_out = NULL;
analog_data_s *_analog_desnapped = NULL; // Data 
analog_data_s analog_data_deadzone = {0}; // Analog data with deadzone coordinates nullified
analog_data_s *_analog_output = NULL;


button_data_s *_buttons = NULL;
button_data_s *_buttons_output = NULL;

typedef struct {
    int8_t tracked_direction;
    int last_pos;
} analog_distance_mem_s;

static analog_distance_mem_s _lx_tracker_mem;
static analog_distance_mem_s _ly_tracker_mem;
static analog_distance_mem_s _rx_tracker_mem;
static analog_distance_mem_s _ry_tracker_mem;

void analog_send_reset()
{
    _lx_tracker_mem.tracked_direction = 0;
    _ly_tracker_mem.tracked_direction = 0;
    _rx_tracker_mem.tracked_direction = 0;
    _ry_tracker_mem.tracked_direction = 0;
}

void analog_init(analog_data_s *in, analog_data_s *out, analog_data_s *desnapped, button_data_s *buttons)
{
    _data_in    = in;
    _data_out   = out;
    _analog_desnapped = desnapped;
    _buttons    = buttons;
    stick_scaling_get_settings();
    stick_scaling_init();

    if (_buttons->button_minus && _buttons->button_plus)
    {
        analog_calibrate_start();
    }
}

void analog_calibrate_start()
{
    rgb_s c = {
        .r = 225,
        .g = 0,
        .b = 0,
    };

    rgb_flash(c.color, -1);

    // Reset scaling distances
    stick_scaling_reset_distances();

    memset(global_loaded_settings.l_sub_angles, 0, sizeof(float)*8);
    memset(global_loaded_settings.r_sub_angles, 0, sizeof(float)*8);

    _analog_all_angles_got = false;
    _analog_centered = false;
    _analog_calibrate = true;
}

void analog_calibrate_stop()
{
    _analog_calibrate = false;

    stick_scaling_set_settings();

    stick_scaling_init();

    //if(webusb_output_enabled())
    //{
    //    #define DUMP_SIZE  (1+(sizeof(float)*8))
    //    uint8_t calibration_dump[DUMP_SIZE] = {0x69};
    //    memcpy(&(calibration_dump[1]), global_loaded_settings.l_angle_distances, DUMP_SIZE-1);
    //    webusb_send_debug_dump(DUMP_SIZE, calibration_dump);
    //}

    rgb_init(global_loaded_settings.rgb_mode, BRIGHTNESS_RELOAD);
}

void analog_calibrate_save()
{

    _analog_calibrate = false;

    stick_scaling_set_settings();

    settings_save_from_core0();

    sleep_ms(200);

    stick_scaling_init();

    rgb_init(global_loaded_settings.rgb_mode, BRIGHTNESS_RELOAD);
}

void analog_calibrate_angle()
{
    stick_scaling_capture_angle(_data_in);
}

void _analog_calibrate_loop()
{
    if(!_analog_centered)
    {
        stick_scaling_capture_center(_data_in);
        _analog_centered = true;
    }
    else if (stick_scaling_capture_distances(_data_in) && !_analog_all_angles_got)
    {
        _analog_all_angles_got = true;
 
        rgb_s c = {
            .r = 0,
            .g = 128,
            .b = 128,
        };
        rgb_flash(c.color, -1);
    }

    if(_buttons->button_home)
    {
        analog_calibrate_save();
    }
}

void analog_get_octoangle_data(uint8_t *axis, uint8_t *octant)
{
    stick_scaling_get_octant_axis_offset(_data_in, axis, octant);
}

void analog_get_subangle_data(uint8_t *axis, uint8_t *octant)
{
    stick_scaling_get_octant_axis(_data_in, axis, octant);
}

#define CLAMP_0_MAX(value) ((value) < 0 ? 0 : ((value) > 4095 ? 4095 : (value)))
#define CLAMP_0_DEADZONE(value) ((value) < 0 ? 0 : ((value) > CENTER ? CENTER : (value)))
#define SCALE_DISTANCE (float)(CENTER-DEADZONE_DEFAULT)
#define SCALE_F (float)(CENTER/SCALE_DISTANCE)

void _analog_process_deadzone(analog_data_s *in, analog_data_s *out)
{
    // Do left side
    float ld = stick_get_distance(in->lx, in->ly, CENTER, CENTER);
    float rd = stick_get_distance(in->rx, in->ry, CENTER, CENTER);

    float left_scale_distance   = CENTER - global_loaded_settings.deadzone_left_center;
    float right_scale_distance  = CENTER - global_loaded_settings.deadzone_right_center;
    float scale_f_left          = (CENTER+global_loaded_settings.deadzone_left_outer)/left_scale_distance;
    float scale_f_right         = (CENTER+global_loaded_settings.deadzone_right_outer)/right_scale_distance;

    if(ld <= global_loaded_settings.deadzone_left_center)
    {
        out->lx = CENTER;
        out->ly = CENTER;
    }
    else
    {
        ld -= global_loaded_settings.deadzone_left_center;

        float la = stick_get_angle(in->lx, in->ly, CENTER, CENTER);

        float lx = 0;
        float ly = 0;

        stick_normalized_vector(la, &lx, &ly);
        float clamped_scaler_left = CLAMP_0_DEADZONE(ld*scale_f_left);
        lx *= clamped_scaler_left;
        ly *= clamped_scaler_left;

        out->lx = CLAMP_0_MAX((int) roundf(lx + CENTER));
        out->ly = CLAMP_0_MAX((int) roundf(ly + CENTER));
    }

    if(rd <= global_loaded_settings.deadzone_right_center)
    {
        out->rx = CENTER;
        out->ry = CENTER;
    }
    else
    {
        rd -= global_loaded_settings.deadzone_right_center;

        float ra = stick_get_angle(in->rx, in->ry, CENTER, CENTER);

        float rx = 0;
        float ry = 0;

        stick_normalized_vector(ra, &rx, &ry);
        float clamped_scaler_right = CLAMP_0_DEADZONE(rd*scale_f_right);
        rx *= clamped_scaler_right;
        ry *= clamped_scaler_right;

        out->rx = CLAMP_0_MAX((int) roundf(rx + CENTER));
        out->ry = CLAMP_0_MAX((int) roundf(ry + CENTER));
    }
}

#define STICK_INTERNAL_CENTER 2048


// DEBUG
void incrementAndReverse(int *value) {
    static bool direction = false;

    if (direction == 1) {
        (*value)+=5;
        if (*value >= 4095) {
            direction = false;
        }
    } else {
        (*value)-=5;
        if (*value <= 0) {
            direction = true;
        }
    }
}

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

// Read analog values
void _analog_read(analog_data_s *data)
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
        data->lx = (uint16_t) getAverage(&ralx);
        data->ly = (uint16_t) getAverage(&raly);
        data->rx = (uint16_t) getAverage(&rarx);
        data->ry = (uint16_t) getAverage(&rary);
    #else
        data->lx = lx;
        data->ly = ly;
        data->rx = rx;
        data->ry = ry;
    #endif
}

void analog_task(uint32_t timestamp)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, _analog_interval, &interval))
    {
        // Read analog sticks
        _analog_read(_data_in);

        if (_analog_calibrate)
        {
            _analog_calibrate_loop();
        }
        // Normal stick reading process
        else
        {
            stick_scaling_process_data(_data_in, &scaled_analog_data);
            
            //#define SNAPBACK_DEBUG 0

            #ifdef SNAPBACK_DEBUG
                _analog_desnapped->lx = scaled_analog_data.lx;
                _analog_desnapped->ly = scaled_analog_data.ly;
                _analog_desnapped->rx = scaled_analog_data.rx;
                _analog_desnapped->ry = scaled_analog_data.ry;
            #else
                snapback_process(timestamp, &scaled_analog_data, _analog_desnapped);
            #endif
            

            // Process deadzones
            _analog_process_deadzone(_analog_desnapped, &analog_data_deadzone);

            // Run distance checks
            _analog_distance_check(analog_data_deadzone.lx, &(_data_out->lx), &_lx_tracker_mem);
            _analog_distance_check(analog_data_deadzone.rx, &(_data_out->rx), &_rx_tracker_mem);
            _analog_distance_check(analog_data_deadzone.ly, &(_data_out->ly), &_ly_tracker_mem);
            _analog_distance_check(analog_data_deadzone.ry, &(_data_out->ry), &_ry_tracker_mem);  
        }
    }
}

