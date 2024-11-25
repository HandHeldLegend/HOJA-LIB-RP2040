#include "extensions/bluetooth.h"
#include "drivers/drivers.h"

bool bluetooth_init(int device_mode, bool pairing_mode)
{
    #if defined(HOJA_BLUETOOTH_INIT)
    HOJA_BLUETOOTH_INIT(device_mode, pairing_mode, bluetooth_callback_handler);
    #endif
}

void bluetooth_task(uint32_t timestamp)
{
    #if defined(HOJA_BLUETOOTH_TASK)
    HOJA_BLUETOOTH_TASK(timestamp)
    #endif
}

// Pass this as our callback handler
void bluetooth_callback_handler(bluetooth_cb_msg_s *msg)
{
    static uint8_t connection_status = 0;
    
    switch(msg->type)
    {
        case BTCB_POWER_CODE:
        break;

        case BTCB_CONNECTION_STATUS:
        break;

        case BTCB_SD_RUMBLE:
            hoja_rumble_msg_s rumble_msg_left = {0};
            hoja_rumble_msg_s rumble_msg_right = {0};

            uint8_t l = msg->data[0];
            uint8_t r = msg->data[1];

            float la = (float) l / 255.0f;
            float ra = (float) r / 255.0f;

            haptics_set_all(0, 0, HOJA_HAPTIC_BASE_LFREQ, la);
        break;

        case BTCB_HD_RUMBLE:
            haptics_rumble_translate(&(msg->data[0]));
        break;
    }
}