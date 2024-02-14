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

#define ARC_MAX_WIDTH 42
#define TRIGGER_MAX_WIDTH (ARC_MAX_WIDTH/2)
#define ARC_MIN_WIDTH 20

#define CENTERVAL 2048
#define MAXVAL 4095
#define ARC_MAX_HEIGHT 200
#define BUFFER_MAX (TRIGGER_MAX_WIDTH+2)
#define ARC_MIN_HEIGHT 1850

// How many measurements we need to
// 'fall' before we activate
#define TRIGGER_THRESHOLD 4

// Additional amount to our decay as a tolerance window
#define DECAY_TOLERANCE 4

#define OUTBUFFER_SIZE 64

typedef struct
{
    int buffer[BUFFER_MAX];
    bool buffer_full;
    uint8_t fifo_idx; // Our index of our current output buffer value.
    uint8_t crossover_idx; // Index of positions where we started an arc. If snapback is detected we start scaling from here
    int trigger_pos; // Position when we started our trigger
    uint8_t trigger; // Trigger counter. If we move the opposite direction more than TRIGGER_THRESHOLD, we apply our snapback
    uint8_t trigger_width; // If we trigger, the width of coordinates that we hit our first trigger
    int8_t  direction; // -1 or 1 to indicate direction of movement
    uint8_t decay_timer; // Timer we can use to decay our snapback scaling
    float   scaler; // Store our scaler to apply to our positions
    bool    rising; // If we are actively processing
    bool    decaying; // If we are actively decaying
} axis_s;

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

int8_t _get_direction(int A, int B)
{
    if(A==B) return 0;

    if(A > B) return 1;
    else return -1;
}

/**
 * Adds value to axis_s and returns the latest value
 */
int _add_axis(int pos, axis_s *a)
{   
    // Get the index of the outbound position
    uint8_t return_idx = (a->fifo_idx+1) % BUFFER_MAX;
    uint8_t last_idx = (!a->fifo_idx) ? (BUFFER_MAX-1) : (a->fifo_idx-1) % BUFFER_MAX;
    int last_pos = a->buffer[last_idx];

    if(!a->buffer_full)
    {
        a->buffer[a->fifo_idx] = pos;
        a->fifo_idx = (a->fifo_idx+1) % BUFFER_MAX;
        if(!a->fifo_idx) a->buffer_full = true;
        return CENTERVAL;
    }

    // Check if we're crossing over
    if(_is_between(CENTERVAL, last_pos, pos))
    {
        
        // Always reset once we cross over
        a->crossover_idx = last_idx;
        a->direction = _get_direction(pos, last_pos);

        a->trigger_width = 0;
        a->rising = true;
        a->decaying = false;
        a->decay_timer = 0;
        a->trigger = 0;
    }
    
    if (a->rising)
    {   
        // Get distance
        int _distance = _get_distance(pos, CENTERVAL);

        // Check if we're too wide
        if( (a->trigger_width >= TRIGGER_MAX_WIDTH) || (_distance >= ARC_MIN_HEIGHT) )
        {
            // Reset here
            a->rising = false;
            a->decaying = false;
            a->decay_timer = 0;
        }
        else
        {
            // If we are moving in the opposite direction
            // increase our trigger counter
            int8_t dir = _get_direction(pos, last_pos);

            if( (dir == a->direction) || (!dir))
            {
                // Restart counter
                a->trigger = 0;
            }
            else 
            {
                if(!a->trigger) a->trigger_pos = last_pos; // Set our trigger pos (to act as height later)

                a->trigger += 1;

                if(a->trigger >= TRIGGER_THRESHOLD)
                {
                    a->decay_timer = a->trigger_width + DECAY_TOLERANCE;
                    a->rising = false;

                    // Calculate the distance from our trigger point to center
                    int trigger_distance = _get_distance(CENTERVAL, a->trigger_pos);

                    // Only start scaling and decaying if we cross the threshold
                    if(trigger_distance >= ARC_MAX_HEIGHT)
                    {
                        
                        a->scaler = (float) ARC_MAX_HEIGHT / (float) trigger_distance;

                        // Get the index of data we want to start modifying
                        uint8_t start_mod_idx = a->crossover_idx;

                        // Modify the width of our positions
                        for(uint8_t i = 0; i < a->trigger_width; i++)
                        {
                            start_mod_idx = (start_mod_idx+1) % BUFFER_MAX;
                            if(start_mod_idx == a->fifo_idx) break;

                            int _od = _get_distance(CENTERVAL, a->buffer[start_mod_idx]);
                            float _nd = (float) _od * a->scaler * (float) a->direction;
                            int _np = CENTERVAL + (int) _nd;
                            a->buffer[start_mod_idx] = _np;

                            
                        }
                        a->decaying = true;
                    }
                    else
                    {
                        // Not snapping back, reset
                        a->rising = false;
                        a->decaying = false;
                    }
                }
            }
        }

        // Increase our trigger width
        a->trigger_width += 1;
    }
    
    if(a->decaying)
    {
        a->decay_timer -= 1;

        int _od = _get_distance(CENTERVAL, pos);
        float _nd = (float) _od * a->scaler;
        int _np = CENTERVAL + ((int) _nd * a->direction);
        
        a->buffer[a->fifo_idx] = _np;

        //a->buffer[a->fifo_idx] = pos;

        if(!a->decay_timer)
        {
            a->decaying = false;
        }
    }
    else
    {
        a->buffer[a->fifo_idx] = pos;
    }

    // Increase fifo idx
    a->fifo_idx = (a->fifo_idx+1) % BUFFER_MAX;
    return a->buffer[return_idx];
}

void snapback_process(uint32_t timestamp, a_data_s *input, a_data_s *output)
{
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
