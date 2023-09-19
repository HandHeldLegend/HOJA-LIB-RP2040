#include "hoja_comms.h"

comms_cb_t _comms_cb = NULL;
static input_mode_t _hoja_input_mode = 0;

void hoja_comms_task(uint32_t timestamp, button_data_s * buttons, a_data_s * analog)
{
    if(_comms_cb != NULL)
    {
        _comms_cb(timestamp, buttons, analog);
    }
}

input_mode_t hoja_comms_current_mode()
{
    return _hoja_input_mode;
}

void hoja_comms_init(input_mode_t input_mode)
{
    _hoja_input_mode = input_mode;
    switch(input_mode)
    {
        case INPUT_MODE_SWPRO:
            _comms_cb = hoja_usb_task;
            hoja_usb_start(INPUT_MODE_SWPRO);
            break;

        case INPUT_MODE_XINPUT:
            _comms_cb = hoja_usb_task;
            hoja_usb_start(INPUT_MODE_XINPUT);
            break;

        case INPUT_MODE_GCUSB:
            _comms_cb = hoja_usb_task;
            hoja_usb_start(INPUT_MODE_GCUSB);
            break;

        case INPUT_MODE_GAMECUBE:
            _comms_cb = gamecube_comms_task;
        break;

        case INPUT_MODE_N64:
            _comms_cb = n64_comms_task;
        break;
    }
}