/**
 * @file stick_scaling.c
 * @brief This file contains the implementation of stick scaling functions.
 */

#include "input/stick_scaling.h"
#include "input/analog.h"

#include "hoja.h"

#include "hal/mutex_hal.h"
#include "utilities/settings.h"

#include "settings_shared_types.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TOTAL_ANGLES 64
#define ADJUSTABLE_ANGLES 16
#define DEFAULT_ANGLES_COUNT 8
#define ANGLE_CHUNK (float)5.625f
#define HALF_ANGLE_CHUNK (float)ANGLE_CHUNK / 2
#define ANGLE_MAPPING_CHUNK (float)22.5f
#define ANGLE_DEFAULT_CHUNK (float)(360.0f / 8.0f)
#define ANALOG_MAX_DISTANCE 2048
#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))
#define NORMALIZE(value, min, max) (((value) - (min)) / ((max) - (min)))
#define MAX_ANGLE_ADJUSTMENT 10
#define MINIMUM_REQUIRED_DISTANCE 1000

bool _sticks_calibrating = false;
uint8_t _l_count = 8;
uint8_t _r_count = 8;

MUTEX_HAL_INIT(_stick_scaling_mutex);

/**
 * @brief Normalizes an angle (in degrees) to the range [0.0, 360.0).
 * * @param angle The input angle, which can be positive or negative.
 * @return The normalized angle in the range [0.0, 360.0).
 */
float _angle_normalize(float angle) {
    angle = fmodf(angle, 360.0f);
    if (angle < 0) angle += 360.0f;
    return angle;
}

float _angle_distance(float angle1, float angle2) {
    float diff = 0;

    if(angle2 > angle1)
    {
        diff = angle2-angle1;
    }
    else 
    {
        diff = angle1-angle2;
    }
    
    // If difference is greater than 180, the shorter path is the other way
    if (diff > 180.0f) {
        diff = 360.0f - diff;
    }
    
    return diff;
}

/**
 * @brief Finds the index of the closest in_angle that is enabled.
 * * @param in_angle The raw input angle.
 * @param config The array of calibration slots.
 * @return The index of the closest enabled slot, or -1 if none are enabled.
 */
int _joy_get_closest_enabled_index(float in_angle, const joyConfigSlot_s config[ADJUSTABLE_ANGLES])
{
    int closest_index = -1;
    float min_diff = 360.0f; // Max possible difference

    for (int i = 0; i < ADJUSTABLE_ANGLES; i++)
    {
        if (!config[i].enabled)
            continue;

        // Compute the shortest angular difference (e.g., diff between 355 and 5 is 10)
        // `fmodf(a - b + 180 + 360, 360) - 180` gives the signed shortest difference
        float diff = fabsf(fmodf(in_angle - config[i].in_angle + 180.0f + 360.0f, 360.0f) - 180.0f);

        if (diff < min_diff)
        {
            min_diff = diff;
            closest_index = i;
        }
    }

    return closest_index;
}

/**
 * @brief Finds the two closest enabled configuration indices that bracket the input angle.
 * * ASSUMPTION: The 'config' array has been partitioned and the enabled slots (0 to 'enabled_count - 1')
 * are sorted by their 'in_angle' in ascending order.
 * * @param in_angle The raw input angle.
 * @param config The array of calibration slots.
 * @param enabled_count The number of enabled, sorted slots in the front of the array.
 * @param index_a Pointer to store the index of the lower bracketing slot.
 * @param index_b Pointer to store the index of the upper bracketing slot.
 * @return true if two bracketing slots were found, false otherwise.
 */
bool _joy_find_bracketing_indices_fast(float in_angle,
                                       const joyConfigSlot_s config[ADJUSTABLE_ANGLES],
                                       int enabled_count,
                                       int *index_a,
                                       int *index_b)
{
    if (enabled_count < 2) return false;

    float normalized_in = _angle_normalize(in_angle);
    int best_a = -1;
    int best_b = -1;
    float min_arc = 360.0f;

    for (int i = 0; i < enabled_count; i++)
    {
        // The next point in the circle (wrapping to 0)
        int next = (i + 1) % enabled_count;

        float start = config[i].in_angle;
        float end = config[next].in_angle;

        // Calculate the angular distance between these two points
        float arc_len = end - start;
        if (arc_len <= 0) arc_len += 360.0f;

        // Check if the current in_angle falls within this wedge
        float offset = normalized_in - start;
        if (offset < 0) offset += 360.0f;

        if (offset <= arc_len)
        {
            // We found the wedge. Because calibration points might be messy,
            // we look for the smallest arc that contains the angle.
            if (arc_len < min_arc)
            {
                min_arc = arc_len;
                best_a = i;
                best_b = next;
            }
        }
    }

    if (best_a != -1)
    {
        *index_a = best_a;
        *index_b = best_b;
        return true;
    }

    return false;
}

// Used for internal sorting by in_angle
int _compare_joyConfigSlot_by_angle(const void *a, const void *b)
{
    const joyConfigSlot_s *slot_a = (const joyConfigSlot_s *)a;
    const joyConfigSlot_s *slot_b = (const joyConfigSlot_s *)b;

    if (slot_a->out_angle < slot_b->out_angle)
        return -1;
    if (slot_a->out_angle > slot_b->out_angle)
        return 1;
    return 0;
}

/**
 * @brief Sorts the array: enabled slots first (sorted by in_angle), then disabled slots.
 * * @param config The array of calibration slots (will be modified).
 * @return The count of enabled slots.
 */
int _joy_validation_sort_and_count(joyConfigSlot_s config[ADJUSTABLE_ANGLES])
{
    int enabled_count = 0;
    int write_ptr = 0;

    // --- Step 1: Partition Enabled and Disabled Slots ---
    // Use a two-pointer approach to move all enabled slots to the front.

    for (int read_ptr = 0; read_ptr < ADJUSTABLE_ANGLES; read_ptr++)
    {
        if (config[read_ptr].enabled)
        {
            // If the slot is enabled, check if a swap is needed
            if (read_ptr != write_ptr)
            {
                // Swap the enabled slot with the slot at the write pointer
                joyConfigSlot_s temp = config[write_ptr];
                config[write_ptr] = config[read_ptr];
                config[read_ptr] = temp;
            }
            write_ptr++;
            enabled_count++;
        }
    }

    // --- Step 2: Sort the Enabled Slots by in_angle ---
    // The enabled slots now occupy indices 0 to (enabled_count - 1).
    if (enabled_count > 1)
    {
        qsort(config, enabled_count, sizeof(joyConfigSlot_s), _compare_joyConfigSlot_by_angle);
    }

    return enabled_count;
}

bool _joy_scale_input(int16_t input[2], analog_meta_s *output, const joyConfigSlot_s config[ADJUSTABLE_ANGLES], uint8_t count)
{
    int index_a, index_b = -1;
    float in_angle = stick_scaling_coordinates_to_angle(input[0], input[1]);
    float in_distance = stick_scaling_coordinate_distance(input[0], input[1]);

    if (!_joy_find_bracketing_indices_fast(in_angle, config, count, &index_a, &index_b)) return false;

    const joyConfigSlot_s *slot_a = &config[index_a];
    const joyConfigSlot_s *slot_b = &config[index_b];

    // Calculate total arcs (ensuring counter-clockwise direction matches bracket search)
    float in_arc = slot_b->in_angle - slot_a->in_angle;
    if (in_arc <= 0) in_arc += 360.0f;

    float out_arc = slot_b->out_angle - slot_a->out_angle;
    if (out_arc <= 0) out_arc += 360.0f;

    // Calculate raw interpolation factor 't'
    float in_diff = in_angle - slot_a->in_angle;
    if (in_diff < 0) in_diff += 360.0f;
    float t = in_diff / in_arc;

    // Apply Angular Deadzones (snapping)
    float dz_a = slot_a->deadzone / 2.0f;
    float dz_b = slot_b->deadzone / 2.0f;
    
    // 'ideal_dist' is the distance along the output arc from out_a
    float ideal_dist = t * out_arc;
    float t_final = t;

    if (ideal_dist <= dz_a) {
        output->angle = slot_a->out_angle;
        t_final = 0.0f; // Snap to slot A for distance scaling
    } else if (ideal_dist >= (out_arc - dz_b)) {
        output->angle = slot_b->out_angle;
        t_final = 1.0f; // Snap to slot B for distance scaling
    } else {
        // Remap the 'live' zone to cover the full output arc smoothly
        float live_arc = out_arc - dz_a - dz_b;
        float t_active = (ideal_dist - dz_a) / live_arc;
        output->angle = _angle_normalize(slot_a->out_angle + (t_active * out_arc));
        t_final = t; 
    }

    // Distance Scaling using snapped 't_final' to ensure stability at axes
    float r_a = slot_a->out_distance / (slot_a->in_distance > 0.0f ? slot_a->in_distance : 1.0f);
    float r_b = slot_b->out_distance / (slot_b->in_distance > 0.0f ? slot_b->in_distance : 1.0f);
    
    float out_ratio = r_a + t_final * (r_b - r_a);
    output->target = slot_a->out_distance + t_final * (slot_b->out_distance - slot_a->out_distance);
    output->distance = in_distance * out_ratio;

    if (output->distance > 4096.0f) output->distance = 4096.0f;
    return true;
}

void _angle_distance_to_fcoordinate(float angle, float distance, float *out)
{
    angle = _angle_normalize(angle);
    float angle_radians = angle * (M_PI / 180.0f);

    out[0] = (float)lroundf(distance * cosf(angle_radians));
    out[1] = (float)lroundf(distance * sinf(angle_radians));

    out[0] = fmaxf(-2048, fminf(2048, out[0])); // Note: If 2048 is your max, you may want to allow 2048 here
    out[1] = fmaxf(-2048, fminf(2048, out[1]));
}

float stick_scaling_coordinates_to_angle(int x, int y)
{
    // Handle special cases to avoid division by zero
    if (x == 0)
    {
        // Vertical lines
        return (y > 0) ? 90.0f : 270.0f;
    }

    // Calculate arctangent and convert to degrees
    float angle_radians = atan2f((float)y, (float)x);

    // Convert to degrees and normalize to 0-360 range
    float angle_degrees = angle_radians * 180.0f / M_PI;

    // Adjust to ensure 0-360 range with positive angles
    if (angle_degrees < 0)
    {
        angle_degrees += 360.0f;
    }

    return angle_degrees;
}

// Get distance to coordinates based on a 0, 0 center
float stick_scaling_coordinate_distance(int x, int y)
{
    // Use Pythagorean theorem to calculate distance
    // Convert to float to ensure floating-point precision
    return sqrtf((float)(x * x + y * y));
}

bool _calibrate_axis(int16_t *in, joyConfigSlot_s slots[ADJUSTABLE_ANGLES])
{
    // Get input distance
    float distance = stick_scaling_coordinate_distance(in[0], in[1]);
    // Get input angle
    float angle = stick_scaling_coordinates_to_angle(in[0], in[1]);

    // Get slot number
    int idx = _joy_get_closest_enabled_index(angle, slots);
    if (idx < 0)
        return false;

    // Assign distance if needed
    if (distance > slots[idx].in_distance)
    {
        slots[idx].in_distance = distance;
    }

    // Return false if an enabled angle is less than 400 distance for the input
    for (int i = 0; i < ADJUSTABLE_ANGLES; i++)
    {
        if ((slots[i].in_distance < 400) && slots[i].enabled)
            return false;
    }

    return true;
}

void _zero_distances(joyConfigSlot_s *slot)
{
    for (int i = 0; i < ADJUSTABLE_ANGLES; i++)
    {
        slot[i].in_distance = 256;
    }
}

void _set_default_configslot(joyConfigSlot_s *slots)
{
    for (int i = 0; i < ADJUSTABLE_ANGLES; i++)
    {
        if (i < 8)
        {
            slots[i].deadzone = 2.0f; // 2 degree deadzone default
            slots[i].enabled = true;
            slots[i].in_angle = i * 45.0f;
            slots[i].out_angle = i * 45.0f;

#if defined(HOJA_ANALOG_DEFAULT_DISTANCE)
            slots[i].in_distance = HOJA_ANALOG_DEFAULT_DISTANCE;
#else
            slots[i].in_distance = 1000.0f;
#endif
            slots[i].out_distance = 2048;
        }
        else
        {
            slots[i].enabled = false;
        }
    }
}

void stick_scaling_calibrate_start(bool start)
{
    MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);
    if (!_sticks_calibrating && start)
    {
        // Reset all to default
        _zero_distances(analog_config->joy_config_l);
        _zero_distances(analog_config->joy_config_r);

        hoja_set_notification_status(COLOR_RED);

        _sticks_calibrating = true;
    }
    else if (_sticks_calibrating && !start)
    {
        hoja_set_notification_status(COLOR_BLACK);
        _sticks_calibrating = false;
    }
    MUTEX_HAL_EXIT(&_stick_scaling_mutex);
}

void stick_scaling_default_check()
{
    if (analog_config->analog_calibration_set == 0xFF)
    {
        analog_config->analog_calibration_set = 0;
    }

    if (analog_config->analog_config_version != CFG_BLOCK_ANALOG_VERSION)
    {
        analog_config->analog_config_version = CFG_BLOCK_ANALOG_VERSION;

        analog_config->analog_calibration_set = 0;

        if(!analog_static.axis_lx)
            analog_config->analog_calibration_set = 1;

        // Set default deadzones
        analog_config->l_deadzone = 144;
        analog_config->r_deadzone = 144;

        analog_config->l_deadzone_outer = 72;
        analog_config->r_deadzone_outer = 72;

        // analog_config->lx_center = 2048;
        // analog_config->ly_center = 2048;
        // analog_config->rx_center = 2048;
        // analog_config->ry_center = 2048;

        analog_config->l_snapback_intensity = 600; // 60.0hz
        analog_config->r_snapback_intensity = 600;

        analog_config->lx_invert = 0;
        analog_config->ly_invert = 0;
        analog_config->rx_invert = 0;
        analog_config->ry_invert = 0;

        analog_config->l_snapback_type = 0; // Default LPF
        analog_config->r_snapback_type = 0; // Default LPF

        // Set to default left setup
        _set_default_configslot(analog_config->joy_config_l);
        _set_default_configslot(analog_config->joy_config_r);
    }
}

// Input and output are based around -2048 to 2048 values with 0 as centers
void stick_scaling_process(analog_data_s *in, analog_data_s *out)
{
    static int16_t in_left[2] = {0};
    static int16_t in_right[2] = {0};

    in_left[0] = in->lx;
    in_left[1] = in->ly;

    in_right[0] = in->rx;
    in_right[1] = in->ry;

    // Float angle and distance data
    analog_meta_s out_left_meta = {0};
    analog_meta_s out_right_meta = {0};

    int16_t out_left[2];
    int16_t out_right[2];

    MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);
    if (_sticks_calibrating)
    {
        bool left_done = _calibrate_axis(in_left, analog_config->joy_config_l);
        bool right_done = _calibrate_axis(in_right, analog_config->joy_config_r);
        out_left[0] = 0;
        out_left[1] = 0;
        out_right[0] = 0;
        out_right[1] = 0;

        if (left_done && right_done)
        {
            hoja_set_notification_status(COLOR_CYAN);
        }
    }
    else
    {
        if (!_joy_scale_input(in_left, &out_left_meta, analog_config->joy_config_l, _l_count))
        {
        }

        if (!_joy_scale_input(in_right, &out_right_meta, analog_config->joy_config_r, _r_count))
        {
        }

        analog_angle_distance_to_coordinate(out_left_meta.angle, out_left_meta.distance, out_left);
        analog_angle_distance_to_coordinate(out_right_meta.angle, out_right_meta.distance, out_right);
    }

    out->lx = out_left[0];
    out->ly = out_left[1];
    out->rx = out_right[0];
    out->ry = out_right[1];

    out->langle = out_left_meta.angle;
    out->ldistance = out_left_meta.distance;
    out->ltarget = out_left_meta.target;

    out->rangle = out_right_meta.angle;
    out->rdistance = out_right_meta.distance;
    out->rtarget = out_right_meta.target;

    MUTEX_HAL_EXIT(&_stick_scaling_mutex);
}

bool stick_scaling_init()
{
    MUTEX_HAL_ENTER_BLOCKING(&_stick_scaling_mutex);

    // Perform a default check. This applies defaults
    // if the config block version is a mismatch
    stick_scaling_default_check();

    // Validate input angles etc. 
    _l_count = _joy_validation_sort_and_count(analog_config->joy_config_l);
    _r_count= _joy_validation_sort_and_count(analog_config->joy_config_r);

    MUTEX_HAL_EXIT(&_stick_scaling_mutex);
    return true;
}
