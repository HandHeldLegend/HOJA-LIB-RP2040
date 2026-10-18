#include "hal/rp2040/boot_radio.h"

#if defined(HOJA_BOOTLOADER) && (HOJA_BOOT_RADIO_SIZE > 0)

#include <string.h>

#include "pico.h"
#include "cyw43_config.h"

#include "hal/rp2040/boot_layout.h"

// Placed by the linker script around the CYW43 firmware (hoja_use_bootloader)
extern const uint8_t __hoja_radio_wifi_start[];
extern const uint8_t __hoja_radio_wifi_end[];
extern const uint8_t __hoja_radio_bt_start[];
extern const uint8_t __hoja_radio_bt_end[];

#define BOOT_RADIO_HEADER                                   \
    {                                                       \
        .magic   = BOOT_RADIO_MAGIC,                        \
        .version = BOOT_RADIO_VERSION,                      \
        .tag     = CYW43_CHIPSET_FIRMWARE_INCLUDE_FILE,     \
        .parts   = {                                        \
            (uint32_t)__hoja_radio_wifi_start,              \
            (uint32_t)__hoja_radio_wifi_end,                \
            (uint32_t)__hoja_radio_bt_start,                \
            (uint32_t)__hoja_radio_bt_end,                  \
        },                                                  \
    }

// The region's own header, linked into the radio region: it stays there with the radio firmware
// when an update replaces only the application
static const boot_radio_header_s __attribute__((used, section(".hoja_radio_header"))) _boot_radio_header =
    BOOT_RADIO_HEADER;

// What this build was linked against
static const boot_radio_header_s _boot_radio_expected = BOOT_RADIO_HEADER;

bool boot_radio_ready(void)
{
    return memcmp((const void *)(XIP_BASE + BOOT_LAYOUT_RADIO_OFFSET), &_boot_radio_expected,
                  sizeof(_boot_radio_expected)) == 0;
}

#else

bool boot_radio_ready(void)
{
    return true;
}

#endif
