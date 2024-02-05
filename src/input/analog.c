#include "analog.h"
#define CENTER 2048
#define DEADZONE 50

const uint32_t _analog_interval = 200;

bool _analog_calibrate = false;
bool _analog_centered = false;
bool _analog_all_angles_got = false;
bool _analog_capture_angle = false;
a_data_s *_data_in = NULL;
a_data_s scaled_analog_data = {0};
a_data_s *_data_out = NULL;
a_data_s *_analog_desnapped = NULL; // Data 
a_data_s analog_data_deadzone = {0}; // Analog data with deadzone coordinates nullified
a_data_s *_analog_output = NULL;


button_data_s *_buttons = NULL;

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

void analog_init(a_data_s *in, a_data_s *out, a_data_s *desnapped, button_data_s *buttons)
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

    rgb_flash(c.color);

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

    cb_hoja_rumble_set(HOJA_HAPTIC_BASE_FREQ, true);
    sleep_ms(500);
    cb_hoja_rumble_set(HOJA_HAPTIC_BASE_FREQ, false);

    stick_scaling_set_settings();

    stick_scaling_init();

    rgb_init(global_loaded_settings.rgb_mode, -1);
}

void analog_calibrate_save()
{

    _analog_calibrate = false;

    cb_hoja_rumble_set(HOJA_HAPTIC_BASE_FREQ, true);
    sleep_ms(500);
    cb_hoja_rumble_set(HOJA_HAPTIC_BASE_FREQ, false);

    stick_scaling_set_settings();

    settings_save();
    sleep_ms(200);

    stick_scaling_init();

    rgb_init(global_loaded_settings.rgb_mode, -1);
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
        rgb_flash(c.color);
    }

    if(_buttons->button_home)
    {
        analog_calibrate_save();
    }
}

void _analog_distance_check(int in, int *out, analog_distance_mem_s *dmem)
{
    // Get movement direction
    // If it's positive, we are moving UP, else DOWN
    bool d = ( (in - dmem->last_pos) > 0 );

    // Handle perfect center
    if(in==2048)
    {
        *out = 2048;
    }
    // If we change direction...
    else if(!dmem->tracked_direction)
    {
        dmem->tracked_direction = d ? 1 : -1;
        // Set output
        *out = in;
    }
    else if (dmem->tracked_direction>0)
    {
        if(in>dmem->last_pos)
        {
            *out            =   in;
            dmem->last_pos  =   in;
        }
    }
    else if (dmem->tracked_direction<0)
    {
        if(in<dmem->last_pos)
        {
            *out            =   in;
            dmem->last_pos  =   in;
        }
    }
    else *out = in;

    // Set last pos
    dmem->last_pos = in;
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
#define SCALE_DISTANCE (float)(CENTER-DEADZONE)
#define SCALE_F (float)(CENTER/SCALE_DISTANCE)

void _analog_process_deadzone(a_data_s *in, a_data_s *out)
{
    // Do left side
    float ld = stick_get_distance(in->lx, in->ly, CENTER, CENTER);
    float rd = stick_get_distance(in->rx, in->ry, CENTER, CENTER);

    if(ld <= DEADZONE)
    {
        out->lx = CENTER;
        out->ly = CENTER;
    }
    else
    {
        ld -= DEADZONE;

        float la = stick_get_angle(in->lx, in->ly, CENTER, CENTER);

        float lx = 0;
        float ly = 0;

        stick_normalized_vector(la, &lx, &ly);
        lx *= ld*SCALE_F;
        ly *= ld*SCALE_F;

        out->lx = CLAMP_0_MAX((int) roundf(lx + CENTER));
        out->ly = CLAMP_0_MAX((int) roundf(ly + CENTER));
    }

    if(rd <= DEADZONE)
    {
        out->rx = CENTER;
        out->ry = CENTER;
    }
    else
    {
        rd -= DEADZONE;

        float ra = stick_get_angle(in->rx, in->ry, CENTER, CENTER);

        float rx = 0;
        float ry = 0;

        stick_normalized_vector(ra, &rx, &ry);
        rx *= rd*SCALE_F;
        ry *= rd*SCALE_F;

        out->rx = CLAMP_0_MAX((int) roundf(rx + CENTER));
        out->ry = CLAMP_0_MAX((int) roundf(ry + CENTER));
    }
}

#define STICK_INTERNAL_CENTER 2048

void analog_task(uint32_t timestamp)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, _analog_interval, &interval))
    {
        // Read analog sticks
        cb_hoja_read_analog(_data_in);

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

