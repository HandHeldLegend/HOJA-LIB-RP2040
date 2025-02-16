#include "devices/bluetooth.h"
#include "hoja_shared_types.h"
#include "devices/rgb.h"
#include "devices/haptics.h"

#include "devices_shared_types.h"
#include "hoja.h"

#include "board_config.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_ESP32HOJA)
    #include "drivers/bluetooth/esp32_hojabaseband.h"
#elif defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER==BLUETOOTH_DRIVER_HAL)
    #include "hal/bluetooth_hal.h"
    #warning "HAL BT IN USE"
#endif

bool bluetooth_mode_start(gamepad_mode_t mode, bool pairing_mode) 
{
#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER>0)

    if(mode==GAMEPAD_MODE_LOAD)
    {
        #if defined(HOJA_BLUETOOTH_INIT_LOAD)
        HOJA_BLUETOOTH_INIT_LOAD();
        hoja_set_notification_status(COLOR_ORANGE);
        return true;
        #endif
    }

    // All other bluetooth modes init normally
    #if defined(HOJA_BLUETOOTH_INIT)
    return HOJA_BLUETOOTH_INIT(mode, pairing_mode, bluetooth_callback_handler);
    #else 
    return false;
    #endif
#endif
}

void bluetooth_mode_stop() 
{
    #if defined(HOJA_BLUETOOTH_STOP)
    HOJA_BLUETOOTH_STOP();
    #endif
}

void bluetooth_mode_task(uint32_t timestamp)
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