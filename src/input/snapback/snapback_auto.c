
#include "input/snapback/snapback_utils.h"
#include "input/analog.h"

#define SNAPBACK_DELAY_TIME_MS      16 
#define SNAPBACK_MAX_IDX            ( (SNAPBACK_DELAY_TIME_MS*1000)/ANALOG_POLL_INTERVAL )
#define SNAPBACK_DELAYED_MAX_IDX    (SNAPBACK_MAX_IDX*2)
#define SNAPBACK_TRIGGER_VELOCITY   1
// Additional amount to our decay as a tolerance window
#define SNAPBACK_DECAY_TOLERANCE    4
#define SNAPBACK_SAFE_HEIGHT        200

#define SNAPBACK_FIXED_POINT_SHIFT 12
#define SNAPBACK_FIXED_POINT_MULT (1<<SNAPBACK_FIXED_POINT_SHIFT)

// How many measurements we need to
// 'fall' before we activate
#define TRIGGER_THRESHOLD 3
#define SNAPBACK_WIDTH_MAX 30
#define SNAPBACK_STORE_HEIGHT 650
#define SNAPBACK_DEADZONE 650
#define SNAPBACK_DISTANCE_THRESHOLD 550
#define CROSSOVER_EXPIRATION 20

// Additional amount to our decay as a tolerance window
#define DECAY_TOLERANCE 4

// If stick travels this much further than trigger point, cancel snapback (intentional input)
#define SNAPBACK_CANCEL_THRESHOLD 50

#define OUTBUFFER_SIZE 64

typedef struct
{
    float last_distance;
    float trigger_distance; // Distance when snapback triggered
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
} sb_auto_axis_s;

sb_auto_axis_s axis[2] = {0};

bool _sbautocenter_trigger_detect(sb_auto_axis_s *axis, int x, int y)
{
    if(axis->stored_x == -7777)
    return false;

    if(sbutil_is_opposite_direction(axis->stored_x, axis->stored_y, x, y))
    {
        axis->stored_x = -7777;
        axis->stored_y = -7777;
        return true;
    }
}

/**
 * Adds value to sb_auto_axis_s and returns the latest value
 */
void _sbauto_add_axis(int16_t x, int16_t y, uint16_t in_distance, uint16_t *out_distance, sb_auto_axis_s *a)
{   
    uint16_t return_distance = in_distance;

    float distance = sbutil_get_2d_distance((int) x, (int) y);

    if(distance >= SNAPBACK_STORE_HEIGHT)
    {
        a->stored_x = (int) x;
        a->stored_y = (int) y;
        a->crossover_expiration = CROSSOVER_EXPIRATION;
    }
    else
    {
        a->crossover_expiration = (!a->crossover_expiration) ? 0 : a->crossover_expiration-1;
    }

    if(_sbautocenter_trigger_detect(a, (int) x, (int) y) && (a->crossover_expiration>0))
    {
        // reset rising
        return_distance = 0;

        a->triggered = false;
        a->rising = true;
        a->decaying = false;
        a->trigger_width = 0;
        a->trigger = 0;
        a->trigger_distance = distance; // Store distance at trigger point
    }
    // Valid snapback wave potentially happening
    else if(a->rising)
    {
        // Cancel if stick travels further than trigger point (intentional input)
        if(distance > a->trigger_distance + SNAPBACK_CANCEL_THRESHOLD)
        {
            a->rising = false;
            a->decaying = false;
            // Don't zero - output real distance
        }
        else
        {
            return_distance = 0;

            a->trigger_width += 1;

            if(sbutil_is_distance_falling(a->last_distance, distance))
            {
                a->triggered = true;
                a->trigger += 1;
            }
            else a->trigger = 0;

            // Check if we should release
            if( (a->trigger_width >= SNAPBACK_WIDTH_MAX) )
            {
                //release
                a->rising = false;

                return_distance = 0;

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
    }
    else if(a->decaying)
    {
        // Cancel if stick travels further than trigger point (intentional input)
        if(distance > a->trigger_distance + SNAPBACK_CANCEL_THRESHOLD)
        {
            a->rising = false;
            a->decaying = false;
            // Don't zero - output real distance
        }
        else
        {
            return_distance = 0;

            a->decay_timer -= 1;
            if(!a->decay_timer)
            {
                a->triggered = false;
                a->rising = false;
                a->decaying = false;
            }
        }
    }

    a->last_distance = distance;
    *out_distance = return_distance;
}


void snapback_auto_process(analog_axis_s *input, analog_axis_s *output)
{
    sb_auto_axis_s *c = &axis[input->axis_idx];

    _sbauto_add_axis(input->x, input->y, input->distance, &(output->distance), c);
    
    output->angle = input->angle;
    if(output->distance>0)
    {
        output->x = input->x;
        output->y = input->y;
    }

    output->target = input->target;
}
