// In HOJA-LIB-RP2040/src/input/snapback.c
#include "snapback.h"
#include <math.h>

#define CENTERVAL 2048


// Beyond this magnitude, the stick is tilted sufficiently far that we are
// worried about snapback when the stick is released.
#define LARGE_MAGNITUDE 2000


// Snapback inputs are smaller than this magnitude.
// TODO: The current value is a wild guess
#define SMALL_MAGNITUDE 1200


// An upper bound on how long it takes the stick to go from tilted to
// almost-neutral, when the stick is released.
// TODO: The current value is a wild guess
#define RELEASE_TIME 5000


// How long we should apply snapback correction.
// TODO: The current value is a wild guess
#define CORRECTION_TIME 10000

typedef struct
{
    uint32_t last_large_magnitude; // The last time we saw a large magnitude
    uint32_t correction_start; // When we started snapback correction mode
    bool correcting; // Whether we are in snapback correction mode or not
} snapback_state;

int _get_distance(int A, int B)
{
    return abs(A-B);
}

int _snapback_correction(uint32_t timestamp, int pos, snapback_state *state)
{
    int _distance = _get_distance(pos, CENTERVAL);


    // Record the timestamps of large magnitudes.
    if(_distance >= LARGE_MAGNITUDE) state->last_large_magnitude = timestamp;


    // Small magnitudes possibly undergo snapback correction.
    if(_distance <= SMALL_MAGNITUDE)
    {
        if(state->correcting)
        {
            // If already correcting, stop correcting after some time.
            if(timestamp - state->correction_start > CORRECTION_TIME)
            {
                state->correcting = false;
            }
        }
        else
        {
            // If not yet correcting, start correcting when quickly going from large to small magnitude.
            if(timestamp - state->last_large_magnitude <= RELEASE_TIME)
            {
                state->correcting = true;
                state->correction_start = timestamp;
            }
        }
    }
    else
    {
        // The magnitude is not small, never apply correction here.
        state->correcting = false;
    }


    if(state->correcting) return CENTERVAL;
    return pos;
}

void snapback_process(uint32_t timestamp, a_data_s *input, a_data_s *output)
{
    static snapback_state lx = {0};
    static snapback_state ly = {0};
    static snapback_state rx = {0};
    static snapback_state ry = {0};


    #if(HOJA_CAPABILITY_ANALOG_STICK_L)
    output->lx = _snapback_correction(timestamp, input->lx, &lx);
    output->ly = _snapback_correction(timestamp, input->ly, &ly);
    #else
    output->lx = CENTERVAL;
    output->ly = CENTERVAL;
    #endif


    #if(HOJA_CAPABILITY_ANALOG_STICK_R)
    output->rx = _snapback_correction(timestamp, input->rx, &rx);
    output->ry = _snapback_correction(timestamp, input->ry, &ry);
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