#include "snapback.h"
#include <math.h>

// How many measurements we need to
// 'fall' before we activate
#define TRIGGER_THRESHOLD 3
#define SNAPBACK_WIDTH_MAX 30
#define SNAPBACK_HEIGHT_MAX 1850
#define SNAPBACK_STORE_HEIGHT 650
#define SNAPBACK_DEADZONE 650
#define SNAPBACK_DISTANCE_THRESHOLD 550
#define CROSSOVER_EXPIRATION 20

// Additional amount to our decay as a tolerance window
#define DECAY_TOLERANCE 4

#define OUTBUFFER_SIZE 64

int _is_opposite_direction(int x1, int y1, int x2, int y2)
{
    // Opposite directions if dot product is negative.
    return (x1 - CENTERVAL) * (x2 - CENTERVAL) + (y1 - CENTERVAL) * (y2 - CENTERVAL) < 0;
}

float _get_2d_distance(int x, int y)
{
  float dx = (float)x - (float)CENTERVAL;
  float dy = (float)y - (float)CENTERVAL;
  return sqrtf(dx * dx + dy * dy);
}

bool _is_distance_falling(float last_distance, float current_distance)
{
    return (current_distance <= last_distance);
}

typedef struct
{
    float last_distance;
    int stored_x;
    int stored_y;
    int16_t crossover_expiration;
    bool triggered;
    uint8_t trigger; // Trigger counter. If we move the opposite direction more than TRIGGER_THRESHOLD, we apply our snapback
    uint8_t trigger_width; // If we trigger, the width of coordinates that we hit our first trigger
    int8_t  direction; // -1 or 1 to indicate direction of movement
    uint8_t decay_timer; // Timer we can use to decay our snapback scaling
    float   scaler; // Store our scaler to apply to our positions
    bool    rising; // If we are actively processing
    bool    decaying; // If we are actively decaying
} axis_s;

bool _center_trigger_detect(axis_s *axis, int x, int y)
{
    if(axis->stored_x < 0)
    return false;

    if(_is_opposite_direction(axis->stored_x, axis->stored_y, x, y))
    {
        axis->stored_x = -1;
        axis->stored_y = -1;
        return true;
    }
}

bool _is_between(int centerValue, int lastPosition, int currentPosition) 
{
    // Check if the current position and last position are on different sides of the center
    if ((currentPosition >= centerValue && lastPosition < centerValue) ||
        (currentPosition < centerValue && lastPosition >= centerValue)) {
        return true;  // Sensor has crossed the center point
    } else {
        return false; // Sensor has not crossed the center point
    }
}

int _get_distance(int A, int B)
{
    return abs(A-B);
}

int8_t _get_direction(int currentPosition, int lastPosition)
{
    if(currentPosition==lastPosition) return 0;

    if(currentPosition > lastPosition) return 1;
    else return -1;
}

/**
 * Adds value to axis_s and returns the latest value
 */
void _add_axis(int x, int y, int *out_x, int *out_y, axis_s *a)
{   
    int return_x = x;
    int return_y = y;

    float distance = _get_2d_distance(x, y);

    if(distance >= SNAPBACK_STORE_HEIGHT)
    {
        a->stored_x = x;
        a->stored_y = y;
        a->crossover_expiration = CROSSOVER_EXPIRATION;
    }
    else
    {
        a->crossover_expiration = (!a->crossover_expiration) ? 0 : a->crossover_expiration-1;
    }

    if(_center_trigger_detect(a, x, y) && (a->crossover_expiration>0))
    {
        // reset rising
        return_x = CENTERVAL;
        return_y = CENTERVAL;

        a->triggered = false;
        a->rising = true;
        a->decaying = false;
        a->trigger_width = 0;
        a->trigger = 0;
    }
    // Valid snapback wave potentially happening
    else if(a->rising)
    {
        return_x = CENTERVAL;
        return_y = CENTERVAL;

        a->trigger_width += 1;

        if(_is_distance_falling(a->last_distance, distance))
        {
            a->triggered = true;
            a->trigger += 1;
        }
        else a->trigger = 0;

        // Check if we should release
        if( (a->trigger_width >= SNAPBACK_WIDTH_MAX) || (distance >= SNAPBACK_HEIGHT_MAX) )
        {
            //release
            a->rising = false;

            return_x = x;
            return_y = y;

            a->decaying = false;
        }
        else if(a->trigger >= TRIGGER_THRESHOLD)
        {
            // We are snapping back
            a->triggered = false;
            a->rising = false;
            a->decaying = true;
            a->decay_timer = a->trigger_width+DECAY_TOLERANCE;

            // Set new stored X and Y
            a->stored_x = x;
            a->stored_y = y;
            // Reset expiration
            a->crossover_expiration = CROSSOVER_EXPIRATION;
        }
    }
    else if(a->decaying)
    {
        return_x = CENTERVAL;
        return_y = CENTERVAL;

        a->decay_timer -= 1;
        if(!a->decay_timer)
        {
            a->triggered = false;
            a->rising = false;
            a->decaying = false;
        }
    }

    a->last_distance = distance;
    *out_x = return_x;
    *out_y = return_y;
}

void snapback_process(uint32_t timestamp, a_data_s *input, a_data_s *output)
{
    static axis_s l = {0};
    static axis_s r = {0};

    #if(HOJA_CAPABILITY_ANALOG_STICK_L)

    _add_axis(input->lx, input->ly, &(output->lx), &(output->ly), &l);

    #else
    output->lx = CENTERVAL;
    output->ly = CENTERVAL;
    #endif

    #if(HOJA_CAPABILITY_ANALOG_STICK_R)
    
    _add_axis(input->rx, input->ry, &(output->rx), &(output->ry), &r);

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

#define CAP_OFFSET 265
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
