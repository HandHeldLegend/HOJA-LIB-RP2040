#include "analog.h"

const uint32_t _analog_interval = 200;

bool _analog_calibrate = false;
bool _analog_centered = false;
bool _analog_all_angles_got = false;
bool _analog_capture_angle = false;
a_data_s *_data_in = NULL;
a_data_s scaled_analog_data = {0};
a_data_s *_data_out = NULL;
a_data_s *_data_buffered = NULL;

button_data_s *_buttons = NULL;

static int _lx_distance = 0;
static int _ly_distance = 0;
static int _rx_distance = 0;
static int _ry_distance = 0;

static bool _lx_direction = false;
static bool _ly_direction = false;
static bool _rx_direction = false;
static bool _ry_direction = false;

void analog_send_reset()
{
    _lx_distance = 0;
    _ly_distance = 0;
    _rx_distance = 0;
    _ry_distance = 0;
}

void analog_init(a_data_s *in, a_data_s *out, a_data_s *buffered, button_data_s *buttons)
{
    _data_in    = in;
    _data_out   = out;
    _data_buffered = buffered;
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
    rgb_set_all(c.color);
    rgb_set_dirty();

    // Reset scaling distances
    stick_scaling_reset_distances();
    _analog_all_angles_got = false;
    _analog_centered = false;
    _analog_calibrate = true;
}

void analog_calibrate_stop()
{
    rgb_load_preset();
    rgb_set_dirty();

    _analog_calibrate = false;

    cb_hoja_rumble_enable(true);
    sleep_ms(200);
    cb_hoja_rumble_enable(false);

    stick_scaling_set_settings();

    stick_scaling_init();
}

void analog_calibrate_save()
{
    rgb_load_preset();
    rgb_set_dirty();

    _analog_calibrate = false;

    cb_hoja_rumble_enable(true);
    sleep_ms(200);
    cb_hoja_rumble_enable(false);

    stick_scaling_set_settings();

    settings_save();
    sleep_ms(200);

    stick_scaling_init();
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
        rgb_set_all(c.color);
        rgb_set_dirty();
    }

    if(_buttons->button_home)
    {
        analog_calibrate_save();
    }
}



void _analog_distance_check(int in, int *out, int *distance, bool *direction)
{
    bool d_internal = (in >= 2048);
    int d = abs(in-2048);

    if(d_internal != *direction)
    {
        *direction = d_internal;
        *distance = d;
        *out = in;
    }
    else if (d > *distance)
    {
        *distance = d;
        *out = in;
    }
    else if (!d)
    {
        *out = 2048;
    }

    // Debug
    //*out = in;
}

void analog_task(uint32_t timestamp)
{
    if (interval_run(timestamp, _analog_interval))
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
            snapback_process(timestamp, &scaled_analog_data, _data_buffered);
            
            // Run distance checks
            _analog_distance_check(_data_buffered->lx, &(_data_out->lx), &_lx_distance, &_lx_direction);
            _analog_distance_check(_data_buffered->rx, &(_data_out->rx), &_rx_distance, &_ly_direction);
            _analog_distance_check(_data_buffered->ly, &(_data_out->ly), &_ly_distance, &_rx_direction);
            _analog_distance_check(_data_buffered->ry, &(_data_out->ry), &_ry_distance, &_ry_direction);
        }
    }
}