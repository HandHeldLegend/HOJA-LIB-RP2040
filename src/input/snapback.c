#include "snapback.h"
#include <math.h>

uint32_t _timestamp_delta(uint32_t new, uint32_t old)
{
    if (new >= old)
    {
        return new - old;
    }
    else if (new < old)
    {
        uint32_t d = 0xFFFFFFFF - old;
        return d + new;
    }

    return 0;
}

#define ARC_MAX_WIDTH 80
#define ARC_MIN_WIDTH 20

#define CENTERVAL 2048
#define MAXVAL 4095
#define ARC_MAX_HEIGHT 250
#define BUFFER_MAX 41
#define ARC_MIN_HEIGHT 1850

#define OUTBUFFER_SIZE 64

typedef struct
{
    int buffer[BUFFER_MAX];
    int last_pos;
    int last_distance;
    bool full;
    bool rising;
    bool falling;
    uint8_t arc_width;
    uint8_t fall_timer;
    // Scaler that gets used until
    // next arc detection begins
    float arc_scaler;
    uint8_t idx;
    uint8_t arc_start_idx;
} axis_s;

inline bool _is_between(int A, int B, int C)
{
    if (B < C)
    {
        return (B < A) && (A <= C);
    }
    else
    {
        return (C < A) && (A <= B);
    }

    return false;
}

/**
 * Adds value to axis_s and returns the latest value
 */
int _add_axis(int pos, axis_s *a)
{
    int ret = CENTERVAL;
    int hold = CENTERVAL;
    float ret_scaler = 1;

    if (a->full)
    {
        ret = a->buffer[a->idx];
    }

    if (a->falling)
    {

        a->fall_timer--;
        int falling_distance = abs(pos - CENTERVAL);

        int dir = (pos < CENTERVAL) ? -1 : 1;
        float nv = (float)falling_distance * a->arc_scaler;
        a->buffer[a->idx] = ((int)nv * dir) + CENTERVAL;

        if (!a->fall_timer)
        {
            a->falling = false;
        }

    }
    else
    {
        // Set normally when not arcing
        a->buffer[a->idx] = pos;
    }

    if (a->rising)
    {
        int this_distance = abs(pos - CENTERVAL); // Get distance to center
        a->arc_width += 1;

        if ( (a->arc_width >= BUFFER_MAX) || (this_distance >= ARC_MIN_HEIGHT))
        {
            a->arc_width = 0;
            a->rising = false;
        }
        // Check if we are decending
        else if (( a->last_distance > this_distance+25) && (a->last_distance > ARC_MAX_HEIGHT))
        {
            
            //printf("SB Height: %d\n", a->last_distance);
            
            float scaler = ARC_MAX_HEIGHT / (float)a->last_distance;
            int si = a->arc_start_idx;

            // We are snapping back
            for (uint8_t i = 0; i < a->arc_width; i++)
            {
                int cp = abs(a->buffer[si] - CENTERVAL);
                int dir = (a->buffer[si] < CENTERVAL) ? -1 : 1;
                float nv = (float)cp * scaler;
                a->buffer[si] = ((int)nv * dir) + CENTERVAL;
                si = (si + 1) % BUFFER_MAX;
            }

            // Set current position to scaler
            int diro = (pos < CENTERVAL) ? -1 : 1;
            float nvo = (float)this_distance * scaler;
            a->buffer[a->idx] = ((int)nvo * diro) + CENTERVAL;

            a->arc_scaler = scaler;
            a->rising = false;
            a->falling = true;
            a->fall_timer = a->arc_width+4;
            a->arc_width = 0;
        }
        else
        {
            a->last_distance = this_distance;
        }
    }

    if (_is_between(CENTERVAL, pos, a->last_pos))
    {
        // We are starting a new arc
        a->rising = true;
        a->fall_timer = 1;
        a->arc_width = 1;
        a->last_distance = 0;
        a->arc_start_idx = a->idx;
    }

    a->last_pos = pos;

    a->idx = (a->idx + 1) % BUFFER_MAX;

    if (!a->idx)
    {
        a->full = true;
    }

    return ret;
}

volatile uint32_t last_time = 0;
volatile uint32_t analog_interval = 0;

void snapback_process(uint32_t timestamp, a_data_s *input, a_data_s *output)
{

    analog_interval = timestamp - last_time;
    last_time = timestamp;

    static axis_s lx = {0};
    static axis_s ly = {0};
    static axis_s rx = {0};
    static axis_s ry = {0};

    #if(HOJA_CAPABILITY_ANALOG_STICK_L)
    output->lx = _add_axis(input->lx, &lx);
    output->ly = _add_axis(input->ly, &ly);
    #else
    output->lx = CENTERVAL;
    output->ly = CENTERVAL;
    #endif

    #if(HOJA_CAPABILITY_ANALOG_STICK_R)
    output->rx = _add_axis(input->rx, &rx);
    output->ry = _add_axis(input->ry, &ry);
    #else
    output->rx = CENTERVAL;
    output->ry = CENTERVAL;
    #endif
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

#define CAP_OFFSET 200
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
