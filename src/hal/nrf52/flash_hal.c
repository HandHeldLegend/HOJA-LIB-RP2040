#include "hal/flash_hal.h"

#include <string.h>

#include "flash_layout.h"
#include "nrf52_hal.h"

// The chip's own flash through the NVMC. The CPU stops while a page erases (about 85 ms) or a word
// is written, interrupts included, so a save is felt as a short gap in the reports.

typedef struct
{
    uint8_t *from;
    uint32_t size;
    uint32_t offset;
} flash_hal_write_s;

static volatile bool _flash_hal_pending = false;
static flash_hal_write_s _flash_hal_write;

static void _flash_hal_wait_ready(void)
{
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
    }
}

static bool _flash_hal_page_offset(uint32_t page, uint32_t *offset)
{
    if (page >= NRF52_FLASH_PAGES)
        return false;
    *offset = NRF52_FLASH_PAGE0_OFFSET - (page * NRF52_FLASH_SECTOR_SIZE);
    return true;
}

bool flash_hal_write(uint8_t *data, uint32_t size, uint32_t page)
{
    uint32_t offset;
    if (_flash_hal_pending || (size > NRF52_FLASH_SECTOR_SIZE) || !_flash_hal_page_offset(page, &offset))
        return false;

    _flash_hal_write.from = data;
    _flash_hal_write.size = size;
    _flash_hal_write.offset = offset;
    _flash_hal_pending = true;
    return true;
}

bool flash_hal_write_sector(uint32_t offset, const uint8_t *data, uint32_t len, uint32_t lockout_ms)
{
    (void)lockout_ms; // No other core to wait for

    if ((offset % NRF52_FLASH_SECTOR_SIZE) || (len > NRF52_FLASH_SECTOR_SIZE) || (len % 4u))
        return false;
    // Only the board's storage area: never the firmware, or a bootloader in front of it
    if ((offset < NRF52_FLASH_STORAGE_OFFSET) || (offset >= NRF52_FLASH_STORAGE_END))
        return false;

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
    _flash_hal_wait_ready();
    NRF_NVMC->ERASEPAGE = offset;
    _flash_hal_wait_ready();

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    _flash_hal_wait_ready();
    for (uint32_t i = 0; i < len; i += 4u)
    {
        uint32_t word;
        memcpy(&word, &data[i], sizeof(word));
        *(volatile uint32_t *)(offset + i) = word;
        _flash_hal_wait_ready();
    }

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    _flash_hal_wait_ready();
    return true;
}

bool flash_hal_read(uint8_t *out, uint32_t size, uint32_t page)
{
    uint32_t offset;
    if ((size > NRF52_FLASH_SECTOR_SIZE) || !_flash_hal_page_offset(page, &offset))
        return false;

    memcpy(out, (const uint8_t *)offset, size);
    return true;
}

void flash_hal_init()
{
}

void flash_hal_task(uint64_t now_us)
{
    (void)now_us;
    if (!_flash_hal_pending)
        return;

    // The settings, padded out to the sector
    static uint8_t page[NRF52_FLASH_SECTOR_SIZE];
    memset(page, 0, sizeof(page));
    memcpy(page, _flash_hal_write.from, _flash_hal_write.size);
    flash_hal_write_sector(_flash_hal_write.offset, page, sizeof(page), 0);

    _flash_hal_pending = false;
}
