#ifndef BOOTLOADER_TUSB_CONFIG_H
#define BOOTLOADER_TUSB_CONFIG_H

// TinyUSB for the bootloader's update mode: device only, one vendor interface it drives itself

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#define CFG_TUD_ENABLED        1
#define CFG_TUSB_RHPORT0_MODE  OPT_MODE_DEVICE
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_CDC    0
#define CFG_TUD_MSC    0
#define CFG_TUD_HID    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0

#endif
