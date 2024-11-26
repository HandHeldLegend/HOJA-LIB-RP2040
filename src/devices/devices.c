#include "devices/devices.h"
#include <stddef.h>

#include "usb/usb.h"

#include "extensions/bluetooth.h"

typedef void(*device_task_t)(uint32_t);

device_task_t _current_device_task = NULL;

device_mode_t device_mode_get()
{

}

// Wrapper to always run the appropriate device task.
void device_task(uint32_t timestamp)
{
    if(_current_device_task!=NULL)
    {
        _current_device_task(timestamp);
    }
}

bool device_start(device_mode_t mode, device_method_t method)
{
    switch(method)
    {
        default:
        case DEVICE_METHOD_USB:
        break;

        case DEVICE_METHOD_WIRED:
        break;

        case DEVICE_METHOD_BLUETOOTH:
        break;
    }
}