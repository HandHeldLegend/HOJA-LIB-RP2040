#include "utilities/boot.h"
#include "hal/sys_hal.h"
#include "input/button.h"

void boot_get_memory(boot_memory_s *out)
{
    boot_memory_s tmp = {0};
    tmp.val = sys_hal_get_bootmemory();

    if(tmp.magic_num != BOOT_MEM_MAGIC)
        out->val = 0x00000000;
    else 
    {
        out->val = tmp.val;
    }
}

void boot_set_memory(boot_memory_s *in)
{
    in->magic_num = BOOT_MEM_MAGIC;
    sys_hal_set_bootmemory(in->val);
}

gamepad_mode_t boot_get_mode_selection()
{
    button_data_s buttons = {0};
    button_access_blocking(&buttons, BUTTON_ACCESS_BOOT_DATA);

    // Choose gamepad boot mode here based on button inputs

    return GAMEPAD_MODE_UNDEFINED;
}