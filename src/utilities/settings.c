#include "utilities/settings.h"
#include "hardware/flash.h"
#include "hoja.h"

#define FLASH_TARGET_OFFSET (1200 * 1024)
#define SETTINGS_BANK_B_OFFSET (FLASH_TARGET_OFFSET)
#define SETTINGS_BANK_A_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE)
#define SETTINGS_BANK_C_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + FLASH_SECTOR_SIZE)
#define BANK_A_NUM 0
#define BANK_B_NUM 1
#define BANK_C_NUM 2 // Bank for power or battery configuration
