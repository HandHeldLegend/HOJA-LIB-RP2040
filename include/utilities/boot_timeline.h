#ifndef UTILITIES_BOOT_TIMELINE_H
#define UTILITIES_BOOT_TIMELINE_H

// Bench builds only (HOJA_BOOT_TIMELINE): when each step of a wireless Auto boot happened. The build
// runs as if on battery even on a cable. Once a link is up (or after a while) the timeline goes to the
// stage header sector and the pad drops into update mode, where the bench reads it back.

#include <stdint.h>

typedef enum
{
    TL_BOOT = 1,       // boot_init done; val: power source
    TL_PHASE,          // Auto moved on; val: the phase
    TL_SWITCH,         // A mode switch carried out; val: the transport
    TL_STACK,          // The Bluetooth stack (re)started
    TL_READY,          // The radio is up
    TL_PAGE,           // The probe calls a saved host; val: its mode
    TL_PROBE_DONE,     // That host answered (1) or not (0)
    TL_HOST,           // A host connected; val: the mode
    TL_DONGLE,         // The dongle connected
    TL_LINE,           // The N64 line check; val: 1 if something holds it high
    TL_BEACON,         // A dongle beacon heard; val: 1 if it's the remembered dongle
    TL_NOTE,           // Anything else; val: free
} boot_timeline_event_t;

#if defined(HOJA_BOOT_TIMELINE) && HOJA_BOOT_TIMELINE
void boot_timeline_mark(boot_timeline_event_t event, uint8_t val);
void boot_timeline_task(uint64_t now_us);
#define BOOT_TIMELINE(event, val) boot_timeline_mark((event), (uint8_t)(val))
#else
#define BOOT_TIMELINE(event, val) ((void)0)
#endif

#endif
