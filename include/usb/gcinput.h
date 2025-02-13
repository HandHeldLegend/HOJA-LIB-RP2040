#ifndef USB_GCINPUT_H
#define USB_GCINPUT_H

#include <stdint.h>
#include "input/button.h"
#include "bsp/board.h"
#include "tusb.h"
#include "utilities/callback.h"

// GC mode definitions
/********************************/
#define GC_HID_LEN 37

#define REPORT_ID_GAMECUBE 0x21

// Input structure for Nintendo GameCube Adapter USB Data
typedef struct
{
    union
    {
        struct
        {
            uint8_t button_a    : 1;
            uint8_t button_b    : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;
            uint8_t dpad_left   : 1; //Left
            uint8_t dpad_right  : 1; //Right
            uint8_t dpad_down   : 1; //Down
            uint8_t dpad_up     : 1; //Up
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t button_start: 1;
            uint8_t button_z    : 1;
            uint8_t button_r    : 1;
            uint8_t button_l    : 1;
            uint8_t blank1      : 4;
        }; 
        uint8_t buttons_2;
    };

  uint8_t stick_x;
  uint8_t stick_y;
  uint8_t cstick_x;
  uint8_t cstick_y;
  uint8_t trigger_l;
  uint8_t trigger_r;

} gc_input_s;

extern const tusb_desc_device_t gc_device_descriptor;
extern const uint8_t gc_hid_report_descriptor[];
extern const uint8_t gc_configuration_descriptor[];

void gcinput_enable(bool enable);
void gcinput_hid_report(uint32_t timestamp, hid_report_tunnel_cb cb);

#endif
