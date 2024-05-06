#ifndef HOJA_DEFINES_H
#define HOJA_DEFINES_H

#define HOJA_CONFIG_CAPABILITIES

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

extern const char* global_string_descriptor[];

#define RGB_PIO pio0
#define RGB_SM 0

#define GAMEPAD_PIO pio1
#define GAMEPAD_SM 0

#define VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR 7

#define HOJA_RGB_DEFAULTS {COLOR_RED.color, COLOR_ORANGE.color, COLOR_YELLOW.color, COLOR_GREEN.color, COLOR_BLUE.color, COLOR_CYAN.color, COLOR_PURPLE.color, \
                            COLOR_RED.color, COLOR_GREEN.color,COLOR_BLUE.color, COLOR_YELLOW.color, COLOR_BLUE.color, \
                            COLOR_WHITE.color, COLOR_WHITE.color, COLOR_WHITE.color, COLOR_WHITE.color}

#endif
