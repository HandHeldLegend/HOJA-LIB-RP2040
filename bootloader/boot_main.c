// The HOJA bootloader: first in flash, ahead of the firmware. On each boot it writes a staged update
// into place, then starts the firmware, or opens the USB update mode when asked for it or when there
// is no firmware to start.

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/watchdog.h"
#include "hardware/structs/nvic.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/systick.h"

#include "hal/rp2040/boot_layout.h"
#include "boot_stage.h"
#include "boot_usb.h"

#define BOOT_APP_BASE    (XIP_BASE + BOOT_LAYOUT_APP_OFFSET)
#define BOOT_APP_VECTORS (XIP_BASE + BOOT_LAYOUT_APP_VECTORS_OFFSET)

// A Pico image's binary info header follows its vector table (68 words on an RP2350, 48 on an
// RP2040) within this many bytes
#define BOOT_BI_SEARCH_LEN 512u
// The header's end marker, in words after its start marker
#define BOOT_BI_END_WORDS  4u

bi_decl(bi_program_description("HOJA bootloader"));

// --- Checks ---

static bool _boot_app_has_binary_info(void)
{
    const uint32_t *word = (const uint32_t *)BOOT_APP_VECTORS;

    for (uint32_t i = 0; i + BOOT_BI_END_WORDS < BOOT_BI_SEARCH_LEN / sizeof(uint32_t); i++)
    {
        if ((word[i] == BINARY_INFO_MARKER_START) && (word[i + BOOT_BI_END_WORDS] == BINARY_INFO_MARKER_END))
            return true;
    }
    return false;
}

// A firmware linked for the application slot: its stack in RAM, its reset handler inside the slot,
// and a Pico image's binary info. Erased flash, a half-written slot (its first sector is written
// last) and an image linked for the start of flash all fail.
static bool _boot_app_valid(void)
{
    const uint32_t *vectors = (const uint32_t *)BOOT_APP_VECTORS;
    uint32_t sp = vectors[0];
    uint32_t reset = vectors[1];

    if ((sp < SRAM_BASE) || (sp > SRAM_END))
        return false;

    if ((reset < BOOT_APP_VECTORS) || (reset >= BOOT_APP_BASE + BOOT_LAYOUT_APP_MAX))
        return false;

    return _boot_app_has_binary_info();
}

// --- Faults ---

// A fault restarts the chip at once and leaves its record behind (the bootloader runs its watchdog
// only in the update mode)
static void __attribute__((noreturn)) _boot_fault(uint32_t what, uint32_t where)
{
    watchdog_hw->scratch[BOOT_FAULT_SCRATCH_IDX] = what;
    watchdog_hw->scratch[BOOT_FAULT_SCRATCH_IDX + 1] = where;
    watchdog_reboot(0, 0, 0);
    for (;;)
    {
    }
}

// The SDK's panic() lands here (PICO_PANIC_FUNCTION)
void __attribute__((noreturn)) boot_panic(const char *fmt, ...)
{
    _boot_fault((uint32_t)fmt, 0);
}

static void __attribute__((used, noreturn)) _boot_hardfault(const uint32_t *frame)
{
    _boot_fault(BOOT_FAULT_HARD, frame[6]); // The stacked PC
}

void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile(
        "mrs r0, msp\n"
        "ldr r1, =_boot_hardfault\n"
        "bx r1\n");
}

// --- Start ---

// The firmware starts as if from reset: none of the bootloader's interrupts may reach it
static void __attribute__((noreturn)) _boot_start_app(void)
{
    const uint32_t *vectors = (const uint32_t *)BOOT_APP_VECTORS;

    __asm volatile("cpsid i" ::: "memory");
    systick_hw->csr = 0;
#if PICO_RP2040
    nvic_hw->icer = 0xFFFFFFFFu;
    nvic_hw->icpr = 0xFFFFFFFFu;
#else
    for (uint32_t i = 0; i < count_of(nvic_hw->icer); i++)
    {
        nvic_hw->icer[i] = 0xFFFFFFFFu;
        nvic_hw->icpr[i] = 0xFFFFFFFFu;
    }
    // The boot ROM set the stack limit for the bootloader; the firmware starts with none, as the
    // SDK's own entry does
    __asm volatile("msr msplim, %0" ::"r"(0u));
#endif
    scb_hw->vtor = (uint32_t)vectors;

    __asm volatile(
        "msr msp, %0\n"
        "cpsie i\n"
        "bx %1\n"
        :
        : "r"(vectors[0]), "r"(vectors[1])
        : "memory");
    __builtin_unreachable();
}

int main(void)
{
    // Taken once: the update mode's own restart then starts the firmware. Any other request is the
    // firmware's, and stays for it.
    if (!boot_request_take(BOOT_REQUEST_UPDATE))
    {
        boot_stage_apply();
        if (_boot_app_valid())
            _boot_start_app();
    }

    boot_usb_run(_boot_app_valid());
}
