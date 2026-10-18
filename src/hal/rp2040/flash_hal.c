#include "hal/flash_hal.h"

#include <string.h>

// Pico SDK specific includes
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "hardware/structs/xip_ctrl.h"
#include "board_config.h"

// With a Bluetooth host stack, settings sit below its pairing storage at the top of flash. The
// legacy offset only fit small firmware (a 1 MB chip wraps 1200 KB to 176 KB).
#if defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_FLOSS
#include "hal/rp2040/bt_flash_bank.h"
#define BT_IN_USE_FLASH 1
#define FLASH_START_OFFSET (BT_FLASH_BANK_OFFSET - FLASH_SECTOR_SIZE)
#elif defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_BTSTACK
#include "pico/btstack_flash_bank.h"
#define BT_IN_USE_FLASH 1 
#define FLASH_START_OFFSET (PICO_FLASH_BANK_STORAGE_OFFSET - FLASH_SECTOR_SIZE)
#else 
// Calculate the last available sectors
#define LAST_FLASH_SECTOR (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_START_OFFSET (1200 * 1024) // LEGACY
#endif 

typedef struct
{
    uint32_t offset;
    const uint8_t *data;
    uint32_t len;
} flash_hal_sector_s;

volatile bool _flash_go = false;
uint8_t *_write_from = NULL;
volatile uint32_t _write_size = 0;
volatile uint32_t _write_offset = 0;

uint32_t _get_sector_offset_read(uint32_t page)
{
    // Calculate offset from the end of flash backwards
    uint32_t target_offset = XIP_BASE + FLASH_START_OFFSET - (page * FLASH_SECTOR_SIZE);
    return target_offset;
}

uint32_t _get_sector_offset_write(uint32_t page)
{
    // Calculate offset from the end of flash backwards
    uint32_t target_offset = FLASH_START_OFFSET - (page * FLASH_SECTOR_SIZE);
    return target_offset;
}

// Write data to flash. Different pages can be used to switch between memory items
// Memory being written must match the device sector size
bool flash_hal_write(uint8_t *data, uint32_t size, uint32_t page) 
{
    if(_flash_go) return false;
    if(size > FLASH_SECTOR_SIZE) return false;

    _write_from = data;
    _write_size = size;
    _write_offset = _get_sector_offset_write(page);
    _flash_go = true;

    return true;
}

// Interrupts off and the other core parked (flash_safe_execute)
static void _flash_hal_sector_op(void *param)
{
    const flash_hal_sector_s *s = (const flash_hal_sector_s *)param;
    flash_range_erase(s->offset, FLASH_SECTOR_SIZE);
    if (s->len)
        flash_range_program(s->offset, s->data, s->len);
}

bool flash_hal_write_sector(uint32_t offset, const uint8_t *data, uint32_t len, uint32_t lockout_ms)
{
    if ((offset % FLASH_SECTOR_SIZE) || (len > FLASH_SECTOR_SIZE) || (len % FLASH_PAGE_SIZE))
        return false;

    flash_hal_sector_s sector = {.offset = offset, .data = data, .len = len};
    return flash_safe_execute(_flash_hal_sector_op, &sector, lockout_ms) == PICO_OK;
}

bool flash_hal_read(uint8_t *out, uint32_t size, uint32_t page) 
{
    if(size > FLASH_SECTOR_SIZE) return false;

    uint32_t offset = _get_sector_offset_read(page);
    const uint8_t *flash_target_contents = (const uint8_t *) (offset);
    memcpy(out, flash_target_contents, size);
    return true;
}

// Should be called from both cores really
void flash_hal_init()
{
    uint core = get_core_num();
    if(core==0)
    {
        flash_safe_execute_core_init();
    }
}

// Run this to apply our flash if
// our flash flag is set
void flash_hal_task(uint64_t now_us)
{
    if(_flash_go)
    {
        // The settings, padded out to the sector
        uint8_t page[FLASH_SECTOR_SIZE] = {0x00};
        memcpy(page, _write_from, _write_size);
        flash_hal_write_sector(_write_offset, page, FLASH_SECTOR_SIZE, UINT32_MAX);

        _write_from = NULL;
        _write_size = 0;
        _write_offset = 0;

        _flash_go = false;
    }
}
