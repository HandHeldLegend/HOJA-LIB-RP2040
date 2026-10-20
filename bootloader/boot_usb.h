#ifndef BOOTLOADER_BOOT_USB_H
#define BOOTLOADER_BOOT_USB_H

// The USB update mode: a vendor interface that speaks the RP2040 boot ROM's PICOBOOT commands, so
// the config app's flasher needs no second protocol, with the MS OS 2.0 descriptors the ROM lacks,
// so Windows binds WinUSB to it without a driver install. Beside it, a UF2 drive (boot_uf2.c).

#include <stdbool.h>
#include <stdint.h>

// Never returns: it restarts the chip when the host asks, or, when nothing was written and a
// firmware is there to go back to (app_valid), once the host goes away
void __attribute__((noreturn)) boot_usb_run(bool app_valid);

// The UF2 drive's way into the flash, by PICOBOOT's rules: the bootloader's own sectors are left
// alone, and the firmware's first sector waits in RAM until the restart, so a write cut short leaves
// no firmware to start. Offsets from the start of flash, sector and page aligned.
void boot_usb_erase_sector(uint32_t offset);
void boot_usb_program_page(uint32_t offset, const uint8_t *data);

// Restart into the firmware once the host has had the last write's status
void boot_usb_restart_after(uint32_t delay_ms);

#endif
