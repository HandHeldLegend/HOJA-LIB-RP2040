#ifndef BOOTLOADER_TUSB_CONFIG_H
#define BOOTLOADER_TUSB_CONFIG_H

// TinyUSB for the bootloader's update mode: device only, a vendor interface it drives itself and
// the UF2 drive (MSC, a sector per transfer)

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#define CFG_TUD_ENABLED        1
#define CFG_TUSB_RHPORT0_MODE  OPT_MODE_DEVICE
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_CDC    0
#define CFG_TUD_MSC    1
#define CFG_TUD_HID    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0

#define CFG_TUD_MSC_EP_BUFSIZE 512

#endif
