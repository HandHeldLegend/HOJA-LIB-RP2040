#include "devices/bluetooth.h"
#include "devices/rgb.h"
#include "drivers/drivers.h"
#include "devices/haptics.h"

#include "devices_shared_types.h"

uint32_t _mode_color = 0;

bool bluetooth_init(int device_mode, bool pairing_mode)
{
    // Handle color options here for differing modes
     switch (device_mode)
    {
    default:
    case DEVICE_MODE_SWPRO:
        _mode_color = COLOR_WHITE.color;
        break;

    case DEVICE_MODE_GCUSB:
        _mode_color = COLOR_CYAN.color;
        break;

    case DEVICE_MODE_XINPUT:
        _mode_color = COLOR_GREEN.color;
        break;

    case DEVICE_MODE_LOAD:
        _mode_color = COLOR_ORANGE.color;
        break;
    }
    //rgb_flash(_mode_color, -1);

    #if defined(HOJA_BLUETOOTH_INIT)
    return HOJA_BLUETOOTH_INIT(device_mode, pairing_mode, bluetooth_callback_handler);
    #endif
}

void bluetooth_load_task(uint32_t timestamp)
{
    
}

void bluetooth_task(uint32_t timestamp)
{
    #if defined(HOJA_BLUETOOTH_TASK)
    HOJA_BLUETOOTH_TASK(timestamp);
    #endif
}

// Pass this as our callback handler
void bluetooth_callback_handler(bluetooth_cb_msg_s *msg)
{
    static uint8_t connection_status = 0;
    //hoja_rumble_msg_s rumble_msg_left = {0};
    //hoja_rumble_msg_s rumble_msg_right = {0};

    uint8_t l = msg->data[0];
    uint8_t r = msg->data[1];

    float la = (float) l / 255.0f;
    float ra = (float) r / 255.0f;
    
    switch(msg->type)
    {
        case BTCB_POWER_CODE:
        break;

        case BTCB_CONNECTION_STATUS:
        break;

        case BTCB_SD_RUMBLE:
            //haptics_set_all(0, 0, HAPTICs_BASE_LFREQ, la);
        break;

        case BTCB_HD_RUMBLE:
            switch_haptics_rumble_translate(&(msg->data[0]));
        break;
    }
}