#ifndef USB_GCINPUT_H
#define USB_GCINPUT_H

#include <stdint.h>
#include "bsp/board.h"
#include "tusb.h"
#include "utilities/callback.h"

// GC mode definitions
/********************************/
#define GC_HID_LEN 37

#define REPORT_ID_GAMECUBE 0x21

extern const tusb_desc_device_t gc_device_descriptor;
extern const uint8_t gc_hid_report_descriptor[];
extern const uint8_t gc_configuration_descriptor[];

void gcinput_enable(bool enable);
void gcinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb);

#endif
