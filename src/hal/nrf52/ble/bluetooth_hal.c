#include "board_config.h"

#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_NRF52)

#include <string.h>

#include "floss/floss.h"

#include "devices/bluetooth.h"
#include "hal/nrf52/ble_controller.h"
#include "hal/nrf52/ble_floss.h"
#include "hal/nrf52/bluetooth.h"
#include "transport/ns2_ble.h"
#include "transport/transport_bt.h"
#include "utilities/bench_log.h"

#if !defined(HOJA_NS2_BLE)
#error "BT_DRIVER_NRF52 has one mode, the Switch 2's (HOJA_NS2_BLE): the build has left it out"
#endif

#if defined(HOJA_BENCH_LOG)
#define BT_HAL_NOTE(text) bench_log_write(text "\n", sizeof(text))
#else
#define BT_HAL_NOTE(text)
#endif

// The controller has no way back down short of the chip's power-off, so the stack starts once and
// stays: a mode that stops leaves the radio idle (no advert, no link)
static bool _bt_hal_up = false;

// The radio's address as the gamepad writes one, most significant byte first
static void _bt_hal_address(uint8_t mac[6])
{
    uint8_t address[6];
    nrf52_ble_controller_address(address);
    for (uint8_t i = 0; i < sizeof(address); i++)
        mac[i] = address[sizeof(address) - 1u - i];
}

bool nrf52_bluetooth_start(bool pairing)
{
    if (!_bt_hal_up)
    {
        const floss_init_t init = {.transport = nrf52_ble_floss_transport()};
        if (floss_init(&init) != FLOSS_OK)
        {
            BT_HAL_NOTE("BT floss_init failed");
            return false;
        }
    }

    uint8_t mac[6];
    _bt_hal_address(mac);
    if (!ns2_ble_start(mac, pairing))
    {
        BT_HAL_NOTE("BT Switch 2 start failed");
        return false;
    }

    if (!_bt_hal_up && (floss_start() != FLOSS_OK))
    {
        BT_HAL_NOTE("BT floss_start failed");
        return false;
    }
    _bt_hal_up = true;
    BT_HAL_NOTE("BT started");
    return true;
}

void nrf52_bluetooth_task(void)
{
    if (!_bt_hal_up)
        return;
    floss_poll();
    ns2_ble_task();
}

bool transport_bt_init(core_params_s *params)
{
    if (!params || (params->core_report_format != CORE_REPORTFORMAT_SWPRO))
    {
        BT_HAL_NOTE("BT asked for a mode it has not got");
        return false;
    }
    if (!nrf52_bluetooth_start((params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0))
        return false;

    // The console knows the controller by the radio's own address, not one made for the mode
    _bt_hal_address(params->transport_dev_mac);
    return true;
}

void transport_bt_stop()
{
    ns2_ble_stop();
}

void transport_bt_task(uint64_t timestamp)
{
    (void)timestamp;
    nrf52_bluetooth_task();
}

bool transport_bt_radio_up(void)
{
    return _bt_hal_up;
}

bool transport_bt_host_paired(core_reportformat_t format)
{
    return (format == CORE_REPORTFORMAT_SWPRO) && ns2_ble_paired();
}

bool transport_bt_forget_supported(void)
{
    return true;
}

bool transport_bt_forget_host(core_reportformat_t format)
{
    if (format != CORE_REPORTFORMAT_SWPRO)
        return false;
    ns2_ble_forget();
    return true;
}

void transport_bt_static_get_caps(transport_bt_static_caps_s *caps)
{
    if (!caps)
        return;
    caps->bdr_supported = 0;
    caps->ble_supported = 1;
    caps->external_update_supported = 0;
}

// The radio is part of the chip the firmware runs on
uint8_t transport_bt_static_part_status(void)
{
    return TRANSPORT_WIRELESS_PART_OK;
}

const char *bluetooth_driver_part_code(void)
{
    return "NRF52833";
}

#endif
