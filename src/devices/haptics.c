#include "devices/haptics.h"

#include "pico/multicore.h"
#include <string.h>

amfm_s ch_l[3] = {0};
int8_t ch_l_count = -1;

amfm_s ch_r[3] = {0};
int8_t ch_r_count = -1;

auto_init_mutex(haptic_mutex);
uint32_t haptic_mutex_owner = 0;

float _rumble_intensity = 1.0f;

void haptics_set_intensity(float intensity)
{
    _rumble_intensity = intensity;
}

float haptics_get_intensity()
{
    return _rumble_intensity;
}

// Function to set more standard rumble
bool haptics_set_all(float f_hi, float a_hi, float f_lo, float a_lo)
{
    a_hi = (a_hi>1) ? 1 : (a_hi<0) ? 0 : a_hi;
    f_hi = (f_hi > 1300) ? 1300 : (f_hi < 40) ? 40 : f_hi;

    a_lo = (a_lo>1) ? 1 : (a_lo<0) ? 0 : a_lo;
    f_lo = (f_lo > 1300) ? 1300 : (f_lo < 40) ? 40 : f_lo;

    amfm_s tmp = {.a_hi = a_hi, .a_lo = a_lo, .f_hi = f_hi, .f_lo = f_lo};

    haptics_set(&tmp, 1, &tmp, 1);
    return true;
}

// Get AMFM struct and return size
// -1 indicates no new data
int8_t haptics_get(bool left, amfm_s* left_out, bool right, amfm_s* right_out)
{
    int8_t status = -1;

    if(mutex_try_enter(&haptic_mutex, &haptic_mutex_owner))
    {
        
        if( (ch_l_count>0) && left)
        {
            status = ch_l_count;
            ch_l_count = -1; // Reset flag
            memcpy(left_out, ch_l, sizeof(amfm_s)*status);
        }

        if( (ch_r_count>0) && right)
        {
            status = ch_r_count;
            ch_r_count = -1; // Reset flag
            memcpy(right_out, ch_r, sizeof(amfm_s)*status);
        }

        mutex_exit(&haptic_mutex);
    }

    return status;
}

// Function to set HD rumble
bool haptics_set(const amfm_s* l_in, int8_t l_count, const amfm_s* r_in, int8_t r_count)
{
    bool status = false;

    mutex_enter_blocking(&haptic_mutex);

    if(l_count>0)
    {
        ch_l_count = l_count;
        memcpy(ch_l, l_in, sizeof(amfm_s)*l_count);
    }

    if(r_count>0)
    {
        ch_r_count = r_count;
        memcpy(ch_r, r_in, sizeof(amfm_s)*r_count);
    }

    mutex_exit(&haptic_mutex);

    return status;
}
