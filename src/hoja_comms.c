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

void hoja_comms_init(input_mode_t input_mode, input_method_t input_method)
{
    _hoja_input_mode = input_mode;

    // Ignore bluetooth mode if we don't support it
    #if (HOJA_CAPABILITY_BLUETOOTH==1)
    if(input_method == INPUT_METHOD_BLUETOOTH)
    {
        _comms_cb = btinput_comms_task;
        if(btinput_init(input_mode)) 
        {
            return;
        }
        // Do not return if we failed to init
    } 
    #endif

    button_data_s _tmp_btn = {0};
    a_data_s _tmp_a = {0};

    switch(input_mode)
    {
        default:
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
            hoja_comms_task(0, &_tmp_btn, &_tmp_a);
            break;

        case INPUT_MODE_N64:
            _comms_cb = n64_comms_task;
            hoja_comms_task(0, &_tmp_btn, &_tmp_a);
            break;

        case INPUT_MODE_SNES:
            _comms_cb = nspi_comms_task;
            break;
    }
}