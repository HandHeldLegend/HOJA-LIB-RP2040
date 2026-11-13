#include "board_config.h"

#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_NRF52)

#include <string.h>

#include "floss/floss.h"

#include "devices/bluetooth.h"
#include "hal/nrf52/ble_controller.h"
#include "hal/nrf52/ble_floss.h"
#include "hal/nrf52/bluetooth.h"
#include "transport/hid_ble.h"
#include "transport/ns2_ble.h"
#include "transport/transport_bt.h"
#include "utilities/bench_log.h"

#if defined(HOJA_BENCH_LOG)
#define BT_HAL_NOTE(text) bench_log_write(text "\n", sizeof(text))
#else
#define BT_HAL_NOTE(text)
#endif

// What the radio is carrying. One to a start of the firmware: each mode has its own services, and
// they are set before the stack starts.
typedef enum
{
    BT_HAL_MODE_NONE,
    BT_HAL_MODE_SWITCH2, // Switch, as a Switch 2's Pro Controller 2 (transport/ns2_ble.h)
    BT_HAL_MODE_HID,     // A mode's HID gamepad (transport/hid_ble.h): SInput
} bt_hal_mode_t;

static bt_hal_mode_t _bt_hal_mode = BT_HAL_MODE_NONE;
static bool _bt_hal_ready = false; // FLOSS is set up
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

// FLOSS, once, for a mode to set its services on. False for a second mode.
static bool _bt_hal_prepare(bt_hal_mode_t mode)
{
    if ((_bt_hal_mode != BT_HAL_MODE_NONE) && (_bt_hal_mode != mode))
    {
        BT_HAL_NOTE("BT is carrying its other mode");
        return false;
    }
    if (_bt_hal_ready)
        return true;

    const floss_init_t init = {.transport = nrf52_ble_floss_transport()};
    if (floss_init(&init) != FLOSS_OK)
    {
        BT_HAL_NOTE("BT floss_init failed");
        return false;
    }
    _bt_hal_ready = true;
    return true;
}

// The stack and the controller, once the mode's services are set
static bool _bt_hal_run(bt_hal_mode_t mode)
{
    if (!_bt_hal_up && (floss_start() != FLOSS_OK))
    {
        BT_HAL_NOTE("BT floss_start failed");
        return false;
    }
    _bt_hal_up = true;
    _bt_hal_mode = mode;
    BT_HAL_NOTE("BT started");
    return true;
}

bool nrf52_bluetooth_start(bool pairing)
{
#if defined(HOJA_NS2_BLE)
    if (!_bt_hal_prepare(BT_HAL_MODE_SWITCH2))
        return false;

    uint8_t mac[6];
    _bt_hal_address(mac);
    if (!ns2_ble_start(mac, pairing))
    {
        BT_HAL_NOTE("BT Switch 2 start failed");
        return false;
    }
    return _bt_hal_run(BT_HAL_MODE_SWITCH2);
#else
    (void)pairing;
    return false;
#endif
}

void nrf52_bluetooth_task(void)
{
    if (!_bt_hal_up)
        return;
    floss_poll();
#if defined(HOJA_NS2_BLE)
    if (_bt_hal_mode == BT_HAL_MODE_SWITCH2)
        ns2_ble_task();
#endif
    if (_bt_hal_mode == BT_HAL_MODE_HID)
        hid_ble_task();
    nrf52_ble_floss_store_task();
}

bool transport_bt_init(core_params_s *params)
{
    if (!params)
        return false;
    const bool pairing = (params->core_boot_flags & COREBOOT_FLAG_PAIR) != 0;

    switch (params->core_report_format)
    {
#if defined(HOJA_NS2_BLE)
    case CORE_REPORTFORMAT_SWPRO:
        if (!nrf52_bluetooth_start(pairing))
            return false;
        // The console knows the controller by the radio's own address, not one made for the mode
        _bt_hal_address(params->transport_dev_mac);
        return true;
#endif

    case CORE_REPORTFORMAT_SINPUT:
    {
        if (!_bt_hal_prepare(BT_HAL_MODE_HID))
            return false;
        uint8_t mac[6];
        _bt_hal_address(mac);
        if (!hid_ble_start(params, mac, pairing))
        {
            BT_HAL_NOTE("BT gamepad start failed");
            return false;
        }
        return _bt_hal_run(BT_HAL_MODE_HID);
    }

    default:
        BT_HAL_NOTE("BT asked for a mode it has not got");
        return false;
    }
}

void transport_bt_stop()
{
#if defined(HOJA_NS2_BLE)
    if (_bt_hal_mode == BT_HAL_MODE_SWITCH2)
        ns2_ble_stop();
#endif
    if (_bt_hal_mode == BT_HAL_MODE_HID)
        hid_ble_stop();
    // What a link put off saving (hal/nrf52/ble_floss.h)
    nrf52_ble_floss_store_flush();
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
    switch (format)
    {
#if defined(HOJA_NS2_BLE)
    case CORE_REPORTFORMAT_SWPRO:
        return ns2_ble_paired();
#endif
    case CORE_REPORTFORMAT_SINPUT:
        return hid_ble_paired();
    default:
        return false;
    }
}

bool transport_bt_forget_supported(void)
{
    return true;
}

bool transport_bt_forget_host(core_reportformat_t format)
{
    switch (format)
    {
#if defined(HOJA_NS2_BLE)
    case CORE_REPORTFORMAT_SWPRO:
        ns2_ble_forget();
        return true;
#endif
    case CORE_REPORTFORMAT_SINPUT:
        hid_ble_forget();
        // With the radio down nothing else would save it
        if (!_bt_hal_up)
            nrf52_ble_floss_store_flush();
        return true;
    default:
        return false;
    }
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
