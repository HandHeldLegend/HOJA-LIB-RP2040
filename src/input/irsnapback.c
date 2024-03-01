#include "snapback.h"
#include <math.h>

#define CENTERVAL 2048
#define ALPHA 0.045f

#define STATE_RAMP_UP (1.0f / 15.0f)
#define STATE_RAMP_DOWN (1.0f / 60.0f)

typedef struct
{
    float filter_last;
    float filter_current;
    float last_normal;
    float blend_decay;
} lpf_state_s;

float low_pass_filter(float input, lpf_state_s *state) {

    // Calculate how much we should increase our blend decay factor
    // First, which region are we in? High or low
    if(input > CENTERVAL)
    {
        // Check if we are moving towards center
        // Increase decay blend if we are
        if(state->last_normal > input)
        {
            state->blend_decay = (state->blend_decay + STATE_RAMP_UP) > 1 ? 1 : state->blend_decay+STATE_RAMP_UP;
        }
        else
        {
            state->blend_decay = (state->blend_decay - STATE_RAMP_DOWN) < 0 ? 0 : state->blend_decay-STATE_RAMP_DOWN;
        }
    }
    else if (input < CENTERVAL)
    {
        if(state->last_normal < input)
        {
            state->blend_decay = (state->blend_decay + STATE_RAMP_UP) > 1 ? 1 : state->blend_decay+STATE_RAMP_UP;
        }
        else
        {
            state->blend_decay = (state->blend_decay - STATE_RAMP_DOWN) < 0 ? 0 : state->blend_decay-STATE_RAMP_DOWN;
        }
    }

    float filtered = ALPHA * input + (1 - ALPHA) * state->filter_last;
    state->filter_last = filtered;
    state->last_normal = input;

    float blend_normal_factor = 1.0f - state->blend_decay;

    float blended_output = (filtered*state->blend_decay) + (input*blend_normal_factor);
    return blended_output;
}

void snapback_process(uint32_t timestamp, a_data_s *input, a_data_s *output)
{
    static lpf_state_s lx_state;
    static lpf_state_s ly_state;
    static lpf_state_s rx_state;
    static lpf_state_s ry_state;

    float x = low_pass_filter((float) input->lx, &lx_state);
    float y = low_pass_filter((float) input->ly, &ly_state);

    float rx = low_pass_filter((float) input->rx, &rx_state);
    float ry = low_pass_filter((float) input->ry, &ry_state);

    output->lx = (int) x;
    output->ly = (int) y;

    output->rx = (int) rx;
    output->ry = (int) ry;
    
}

uint8_t _snapback_report[64] = {0};

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

bool _snapback_add_value(int val)
{
    static uint8_t _idx = 0;
    int t = val >> 4;
    _snapback_report[_idx + 2] = (uint8_t)t;
    _idx += 1;
    if (_idx > 61)
    {
        _idx = 0;
        return true;
    }
    return false;
}

#define CAP_OFFSET 260
#define UPPER_CAP 4095 - CAP_OFFSET
#define LOWER_CAP 0 + CAP_OFFSET
#define CAP_INTERVAL 1000

void snapback_webcapture_task(uint32_t timestamp, a_data_s *data)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, CAP_INTERVAL, &interval))
    {
        static bool _capturing = false;
        static int *selection = NULL;
        static bool _got_selection = false;
        static uint8_t _selection_idx = 0;

        if (_capturing)
        {
            if (_snapback_add_value(*selection))
            {
                // Send packet
                _snapback_report[0] = WEBUSB_CMD_ANALYZE_STOP;
                _snapback_report[1] = _selection_idx;

                /*DEBUG
                _snapback_report[2] = (analog_interval>>24)&0xFF;
                _snapback_report[3] = (analog_interval>>16)&0xFF;
                _snapback_report[4] = (analog_interval>>8)&0xFF;
                _snapback_report[5] = (analog_interval&0xFF);*/
                if (webusb_ready_blocking(4000))
                {
                    tud_vendor_n_write(0, _snapback_report, 64);
                    tud_vendor_n_flush(0);
                }
                _capturing = false;
                _got_selection = false;
            }
        }
        else if (!_got_selection)
        {
            if (data->lx >= UPPER_CAP || data->lx <= LOWER_CAP)
            {
                selection = &(data->lx);
                _got_selection = true;
                _selection_idx = 0;
            }
            else if (data->ly >= UPPER_CAP || data->ly <= LOWER_CAP)
            {
                selection = &(data->ly);
                _got_selection = true;
                _selection_idx = 1;
            }
            else if (data->rx >= UPPER_CAP || data->rx <= LOWER_CAP)
            {
                selection = &(data->rx);
                _got_selection = true;
                _selection_idx = 2;
            }
            else if (data->ry >= UPPER_CAP || data->ry <= LOWER_CAP)
            {
                selection = &(data->ry);
                _got_selection = true;
                _selection_idx = 3;
            }
        }
        else if (_got_selection)
        {
            if (*selection<UPPER_CAP && * selection> LOWER_CAP)
            {
                _capturing = true;
            }
        }
    }
}
