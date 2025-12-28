#include "input/snapback.h"

#include "input/snapback/snapback_utils.h"
#include "input/snapback/snapback_auto.h"
#include "input/snapback/snapback_lpf.h"

#include "input/analog.h"
#include "usb/webusb.h"
#include "utilities/settings.h"

#include <string.h>

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

void snapback_webcapture(analog_data_s *data)
{

    static bool _capturing = false;
    static uint16_t *selection = NULL;
    static bool _got_selection = false;
    static uint8_t _selection_idx = 0;

    uint16_t lx = (uint16_t) (data->lx + 2048);
    uint16_t ly = (uint16_t) (data->ly + 2048);
    uint16_t rx = (uint16_t) (data->rx + 2048);
    uint16_t ry = (uint16_t) (data->ry + 2048);

    if (_capturing)
    {
        if (_snapback_add_value(*selection))
        {
            // Send packet
            _snapback_report[0] = WEBUSB_ANALOG_DUMP;
            _snapback_report[1] = _selection_idx;

            webusb_send_bulk(_snapback_report, 64);

            _capturing = false;
            _got_selection = false;
        }
    }
    else if (!_got_selection)
    {
        if (lx >= UPPER_CAP || lx <= LOWER_CAP)
        {
            selection = &lx;
            _got_selection = true;
            _selection_idx = 0;
        }
        else if (ly >= UPPER_CAP || ly <= LOWER_CAP)
        {
            selection = &(ly);
            _got_selection = true;
            _selection_idx = 1;
        }
        else if (rx >= UPPER_CAP || rx <= LOWER_CAP)
        {
            selection = &(rx);
            _got_selection = true;
            _selection_idx = 2;
        }
        else if (ry >= UPPER_CAP || ry <= LOWER_CAP)
        {
            selection = &(ry);
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

void snapback_process(analog_data_s *input, analog_data_s *output)
{
    static analog_axis_s l = {0};
    static analog_axis_s r = {0};
    static analog_axis_s l_out = {0};
    static analog_axis_s r_out = {0};

    sbutil_data_to_axis(input, &l, &r);

    switch(analog_config->l_snapback_type)
    {   
        // LPF
        default:
        snapback_lpf_process(&l, &l_out);
        break;

        // Auto
        case 1:
        snapback_auto_process(&l, &l_out);
        break;

        // Off
        case 2:
        l_out.x          = l.x;
        l_out.y          = l.y;
        l_out.distance   = l.distance;
        l_out.target     = l.target;
        l_out.angle      = l.angle;
        break;
    }

    switch(analog_config->r_snapback_type)
    {   
        // LPF
        default:
        snapback_lpf_process(&r, &r_out);
        break;

        // Auto
        case 1:
        snapback_auto_process(&r, &r_out);
        break;

        // Off
        case 2:
        r_out.x          = r.x;
        r_out.y          = r.y;
        r_out.distance   = r.distance;
        r_out.target     = r.target;
        r_out.angle      = r.angle;
        break;
    }

    // Copy output data
    sbutil_axis_to_data(&l_out, &r_out, output);

    if(webusb_outputting_check())
        snapback_webcapture(output);
}
