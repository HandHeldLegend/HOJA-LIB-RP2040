#include "reboot.h"

#define SCRATCH_OFFSET 0xC
#define MAX_INDEX     7
#define WD_READOUT_IDX 5



void _scratch_set(uint8_t index, uint32_t value) {
    if (index > MAX_INDEX) {
        // Handle the error here. For simplicity, we'll just return in this example.
        return;
    }
    *((volatile uint32_t *) (WATCHDOG_BASE + SCRATCH_OFFSET + (index * 4))) = value;
}

uint32_t _scratch_get(uint8_t index) {
    if (index > MAX_INDEX) {
        // Handle the error, maybe by returning an error code or logging a message.
        // Here we just return 0 as a simple example.
        return 0;
    }
    uint32_t v = *((volatile uint32_t *) (WATCHDOG_BASE + SCRATCH_OFFSET + (index * 4)));
    _scratch_set(WD_READOUT_IDX, 0);
    return v;
}

void reboot_with_memory(uint32_t value)
{
    hoja_reboot_memory_u reboot_memory = {0};
    reboot_memory.value = value;
    _scratch_set(WD_READOUT_IDX, reboot_memory.value);

    #if(HOJA_CAPABILITY_RGB == 1)
        rgb_shutdown_start(true);
    #else
        hoja_shutdown_instant();
    #endif
}

uint32_t reboot_get_memory()
{
    return _scratch_get(WD_READOUT_IDX);
}

