#ifndef BOOTLOADER_BOOT_USB_H
#define BOOTLOADER_BOOT_USB_H

// The USB update mode: a vendor interface that speaks the RP2040 boot ROM's PICOBOOT commands, so
// the config app's flasher needs no second protocol, with the MS OS 2.0 descriptors the ROM lacks,
// so Windows binds WinUSB to it without a driver install.

#include <stdbool.h>

// Never returns: it restarts the chip when the host asks, or, when nothing was written and a
// firmware is there to go back to (app_valid), once the host goes away
void __attribute__((noreturn)) boot_usb_run(bool app_valid);

#endif
