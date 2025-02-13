#include "board_config.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_HAL)

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_chipset_cyw43.h"
#include "btstack_config.h"

#endif