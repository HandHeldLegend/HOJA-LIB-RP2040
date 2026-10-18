#include "utilities/boot_timeline.h"

#if defined(HOJA_BOOT_TIMELINE) && HOJA_BOOT_TIMELINE

#include <string.h>
#include "pico/time.h"
#include "hal/flash_hal.h"
#include "hal/sys_hal.h"
#include "hal/rp2040/boot_layout.h"

#define TL_MAGIC     0x314E4C54u // "TLN1"
#define TL_MAX       120
// After the first link, a little longer for anything that follows it
#define TL_AFTER_MS  1500
#define TL_GIVEUP_MS 40000

typedef struct
{
    uint32_t magic;
    uint32_t count;
    uint32_t entries[TL_MAX]; // event << 24 | val << 16 | ms (since boot, 16 bits)
} timeline_s;

static timeline_s _tl = {.magic = TL_MAGIC};
static uint32_t _linked_ms = 0;
static uint32_t _ms[TL_MAX]; // full ms, for runs past 65 s

void boot_timeline_mark(boot_timeline_event_t event, uint8_t val)
{
    const uint32_t ms = to_ms_since_boot(get_absolute_time());
    if (_tl.count < TL_MAX)
    {
        _ms[_tl.count] = ms;
        _tl.entries[_tl.count++] = ((uint32_t)event << 24) | ((uint32_t)val << 16) | (ms & 0xFFFFu);
    }
    if ((event == TL_HOST || event == TL_DONGLE) && !_linked_ms)
        _linked_ms = ms ? ms : 1;
}

void boot_timeline_task(uint64_t now_us)
{
    static bool done = false;
    if (done)
        return;
    const uint32_t ms = (uint32_t)(now_us / 1000);
    if (!(_linked_ms && ms - _linked_ms >= TL_AFTER_MS) && ms < TL_GIVEUP_MS)
        return;
    done = true;

    static uint8_t sector[4096];
    memset(sector, 0xFF, sizeof(sector));
    memcpy(sector, &_tl, sizeof(_tl));
    flash_hal_write_sector(BOOT_LAYOUT_STAGE_HEADER_OFFSET, sector, sizeof(sector), 100);
    sys_hal_bootloader();
}

#endif
