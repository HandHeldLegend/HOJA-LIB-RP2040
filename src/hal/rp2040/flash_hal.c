#include "hal/flash_hal.h"

#include <string.h>

// Pico SDK specific includes
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "hardware/structs/xip_ctrl.h"

// FLASH_SECTOR_SIZE 4096 bytes 
// FLASH_PAGE_SIZE 256 bytes
// Offset needs to be added to XIP_BASE

#define STORAGE_CMD_TOTAL_BYTES 4

uint32_t _get_total_flash_size_bytes()
{
    uint8_t txbuf[STORAGE_CMD_TOTAL_BYTES] = {0x9f};
    uint8_t rxbuf[STORAGE_CMD_TOTAL_BYTES] = {0};
    flash_do_cmd(txbuf, rxbuf, STORAGE_CMD_TOTAL_BYTES);

    return (uint32_t) 1 << rxbuf[3];
}

uint32_t _get_total_flash_sectors() 
{
    uint32_t capacity = _get_total_flash_size_bytes();
    return capacity / FLASH_SECTOR_SIZE;
}

uint32_t _get_sector_offset(uint32_t page)
{
    uint32_t sectors = _get_total_flash_sectors()-1;
    uint32_t first_page_offset = XIP_BASE + (sectors * FLASH_SECTOR_SIZE);
    uint32_t target_offset = first_page_offset - (page * FLASH_SECTOR_SIZE);
}

// This function will be called when it's safe to call flash_range_erase
static void _call_flash_range_erase(void *param) {
    uint32_t offset = (uint32_t)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

// This function will be called when it's safe to call flash_range_program
static void _call_flash_range_program(void *param) {
    uint32_t offset = ((uintptr_t*)param)[0];
    const uint8_t *data = (const uint8_t *)((uintptr_t*)param)[1];
    flash_range_program(offset, data, FLASH_PAGE_SIZE);
}

// Write data to flash. Different pages can be used to switch between memory items
// Memory being written must match the device sector size
bool flash_hal_write(const uint8_t *data, uint32_t size, uint32_t page) 
{
    if(size>FLASH_SECTOR_SIZE) return false;

    uint32_t offset = _get_sector_offset(page);

    int rc = flash_safe_execute(_call_flash_range_erase, (void*)offset, UINT32_MAX);
    hard_assert(rc == PICO_OK);

    uintptr_t params[] = { offset, (uintptr_t)data};
    rc = flash_safe_execute(_call_flash_range_program, params, UINT32_MAX);
    hard_assert(rc == PICO_OK);
}

bool flash_hal_read(uint8_t *out, uint32_t size, uint32_t page) 
{
    if(size>FLASH_SECTOR_SIZE) return false;

    uint32_t offset = _get_sector_offset(page);

    uint8_t *flash_target_contents = (uint8_t *) (offset);

    memcpy(out, flash_target_contents, size);
    return true;
}

// Should be called from both cores really
void flash_hal_init()
{
    uint core = get_core_num();

    if(core==1)
    {
        flash_safe_execute_core_init();
    }
    else 
    {

    }
}
