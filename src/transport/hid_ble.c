#include "transport/hid_ble.h"

#include "board_config.h"

#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_NRF52)

#include <string.h>

#include "floss/floss.h"

#include "devices/battery.h"
#include "hal/sys_hal.h"
#include "hoja.h"
#include "transport/transport.h"
#include "utilities/tasks.h"

// SInput's reports (SINPUT-LIB-HID), their lengths without the id: the gamepad, a command's reply,
// and the host's commands. The reply comes first: when both wait, it goes out ahead of the gamepad.
#define HID_BLE_SINPUT_INPUT_ID   0x01u
#define HID_BLE_SINPUT_REPLY_ID   0x02u
#define HID_BLE_SINPUT_OUTPUT_ID  0x03u
#define HID_BLE_SINPUT_INPUT_LEN  63u
#define HID_BLE_SINPUT_OUTPUT_LEN 47u

// An ATT attribute's limit, which a report descriptor has to fit
#define HID_BLE_REPORT_MAP_MAX_LEN 512u

// The ids a host knows the gamepad by are its USB ones
#define HID_BLE_VENDOR_ID_SOURCE_USB 2u
#define HID_BLE_VERSION              0x0100u

// The host is asked for the shortest connection interval, 7.5 ms (in units of 1.25 ms), and no more
// than 11.25: a report goes at each one
#define HID_BLE_INTERVAL_MIN 6u
#define HID_BLE_INTERVAL_MAX 6u

// The gamepad reads its buttons once to a cycle of its task loop, which left alone starts every
// 8 ms. Cycles run this often instead, so the report sent at a connection event is no older.
#define HID_BLE_CYCLE_US 2000u

// The mode's address is the radio's with its lowest byte turned over, as a random static address
// (the two top bits set)
#define HID_BLE_ADDRESS_TURN   0xFFu
#define HID_BLE_ADDRESS_STATIC 0xC0u

// Off the cable with no host for this long, the gamepad turns itself off
#define HID_BLE_ALONE_OFF_US (5u * 60u * 1000000u)

static const floss_hogp_report_t _hid_ble_sinput_reports[] = {
    {.id = HID_BLE_SINPUT_REPLY_ID, .type = FLOSS_HID_REPORT_INPUT, .len = HID_BLE_SINPUT_INPUT_LEN},
    {.id = HID_BLE_SINPUT_INPUT_ID, .type = FLOSS_HID_REPORT_INPUT, .len = HID_BLE_SINPUT_INPUT_LEN},
    {.id = HID_BLE_SINPUT_OUTPUT_ID, .type = FLOSS_HID_REPORT_OUTPUT, .len = HID_BLE_SINPUT_OUTPUT_LEN},
};

static core_params_s *_hid_ble_params = NULL;
static floss_hogp_config_t _hid_ble_cfg;
static bool _hid_ble_started = false;
static bool _hid_ble_stopped = false;
static bool _hid_ble_linked = false;
static uint64_t _hid_ble_alone_since_us = 0; // 0 while a host has the link

// The gamepad's own view of the link: its status light, and what waits on a connection
static void _hid_ble_connection(bool connected)
{
    transport_evt_cb((tp_evt_s){
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {
            .connection = connected ? TP_CONNECTION_CONNECTED : TP_CONNECTION_DISCONNECTED,
        },
    });
}

// Pairing is open to a new host when the gamepad was started to pair, and while it has none
static void _hid_ble_pairable(bool pairable)
{
    floss_smp_config_t smp;
    floss_smp_config_defaults(&smp);
    smp.pairable = pairable;
    floss_smp_set_config(&smp);
}

static void _hid_ble_on_connected(uint16_t handle, const floss_addr_t *host)
{
    (void)handle;
    (void)host;
    _hid_ble_linked = true;
    _hid_ble_connection(true);
}

static void _hid_ble_on_disconnected(uint16_t handle, uint8_t reason)
{
    (void)handle;
    (void)reason;
    _hid_ble_linked = false;
    _hid_ble_connection(false);
    if (!_hid_ble_stopped && !floss_le_adv_active())
        floss_le_adv_start();
}

// The host's commands (rumble, lights, the features request) go to the mode as they do from USB,
// id first
static void _hid_ble_on_output(uint8_t report_id, const uint8_t *data, uint16_t len)
{
    uint8_t report[1u + FLOSS_HID_MAX_REPORT];
    if (!_hid_ble_params || !_hid_ble_params->core_report_tunnel || (len > FLOSS_HID_MAX_REPORT))
        return;

    report[0] = report_id;
    memcpy(&report[1], data, len);
    _hid_ble_params->core_report_tunnel(report, (uint16_t)(len + 1u));
}

// Right before each connection event's report goes: the mode's newest one. The id picks the
// characteristic; it is not sent.
static void _hid_ble_on_tick(void)
{
    core_report_s report;
    if (!core_get_generated_report(&report) || (report.size < 2u))
        return;

    floss_hogp_set_input(report.data[0], &report.data[1], (uint16_t)(report.size - 1u));
    tasks_mark_sent();
}

bool hid_ble_start(core_params_s *params, const uint8_t radio_mac[6], bool pairing)
{
    // Carrying on after a stop, or started twice: the services are as they were
    if (_hid_ble_started)
    {
        _hid_ble_stopped = false;
        if (!floss_le_link_count() && !floss_le_adv_active())
            floss_le_adv_start();
        return true;
    }

    if (!params || !params->hid_device || (params->core_report_format != CORE_REPORTFORMAT_SINPUT))
        return false;

    const core_hid_device_t *hid = params->hid_device;
    if (!hid->hid_report_descriptor || !hid->hid_report_descriptor_len ||
        (hid->hid_report_descriptor_len > HID_BLE_REPORT_MAP_MAX_LEN))
        return false;

    const hoja_config_s *board = hoja_config_get();

    floss_le_set_name(hid->name);
    floss_le_set_appearance(FLOSS_LE_APPEARANCE_GAMEPAD);

    floss_hogp_config_defaults(&_hid_ble_cfg);
    _hid_ble_cfg.report_map = hid->hid_report_descriptor;
    _hid_ble_cfg.report_map_len = (uint16_t)hid->hid_report_descriptor_len;
    _hid_ble_cfg.reports = _hid_ble_sinput_reports;
    _hid_ble_cfg.report_count = (uint8_t)(sizeof(_hid_ble_sinput_reports) / sizeof(_hid_ble_sinput_reports[0]));
    _hid_ble_cfg.manufacturer = (board && board->device_maker) ? board->device_maker : NULL;
    _hid_ble_cfg.model = hid->name;
    _hid_ble_cfg.vendor_id_source = HID_BLE_VENDOR_ID_SOURCE_USB;
    _hid_ble_cfg.vendor_id = hid->vid;
    _hid_ble_cfg.product_id = hid->pid;
    _hid_ble_cfg.version = HID_BLE_VERSION;
    _hid_ble_cfg.report_interval_us = FLOSS_HOGP_INTERVAL_CONN;

    static const floss_hogp_callbacks_t callbacks = {
        .on_connected = _hid_ble_on_connected,
        .on_disconnected = _hid_ble_on_disconnected,
        .on_output_report = _hid_ble_on_output,
        .on_report_tick = _hid_ble_on_tick,
    };
    if (floss_hogp_init(&_hid_ble_cfg, &callbacks) != FLOSS_OK)
        return false;

    _hid_ble_pairable(pairing || !hid_ble_paired());

    floss_le_link_policy_t policy;
    floss_le_link_policy_defaults(&policy);
    policy.preferred.interval_min = HID_BLE_INTERVAL_MIN;
    policy.preferred.interval_max = HID_BLE_INTERVAL_MAX;
    floss_le_set_link_policy(&policy);

    // FLOSS writes an address least significant byte first
    floss_addr_t address;
    for (uint8_t i = 0; i < sizeof(address.b); i++)
        address.b[i] = radio_mac[sizeof(address.b) - 1u - i];
    address.b[0] ^= HID_BLE_ADDRESS_TURN;
    address.b[sizeof(address.b) - 1u] |= HID_BLE_ADDRESS_STATIC;
    floss_le_set_static_address(&address);

    floss_le_adv_params_t advert;
    floss_le_adv_params_defaults(&advert);
    advert.own_addr_type = FLOSS_LE_ADDR_RANDOM;
    floss_le_adv_set_params(&advert);

    static const uint16_t services[] = {FLOSS_GATT_SVC_HID};
    if (floss_le_adv_set_standard(services, (uint8_t)(sizeof(services) / sizeof(services[0]))) != FLOSS_OK)
        return false;

    _hid_ble_params = params;
    _hid_ble_started = true;
    _hid_ble_stopped = false;
    return floss_le_adv_start() == FLOSS_OK;
}

void hid_ble_stop(void)
{
    if (!_hid_ble_started || _hid_ble_stopped)
        return;

    _hid_ble_stopped = true;
    floss_le_adv_stop();
    const uint16_t link = floss_le_link_handle_at(0);
    if (link != 0xFFFFu)
        floss_le_disconnect(link, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
}

bool hid_ble_linked(void)
{
    return _hid_ble_linked;
}

// Read from what FLOSS has saved, so it answers before the stack is started too
bool hid_ble_paired(void)
{
    for (uint16_t i = 0; i < FLOSS_MAX_BONDS; i++)
    {
        uint8_t first;
        if (floss_hal_store_read((uint16_t)(FLOSS_SMP_STORE_KEY_BOND_BASE + i), &first, sizeof(first)) > 0)
            return true;
    }
    return false;
}

void hid_ble_forget(void)
{
    if (!_hid_ble_started)
    {
        for (uint16_t i = 0; i < FLOSS_MAX_BONDS; i++)
            floss_hal_store_delete((uint16_t)(FLOSS_SMP_STORE_KEY_BOND_BASE + i));
        return;
    }

    // Their keys are gone: a link to one goes, and any host may pair
    floss_smp_bond_delete_all();
    _hid_ble_pairable(true);
    const uint16_t link = floss_le_link_handle_at(0);
    if (link != 0xFFFFu)
        floss_le_disconnect(link, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
}

void hid_ble_task(void)
{
    if (!_hid_ble_started || _hid_ble_stopped)
        return;

    static uint64_t cycle_clock = 0;
    const uint64_t now_us = sys_hal_now_us();
    const uint64_t clock = now_us / HID_BLE_CYCLE_US;
    if (clock != cycle_clock)
    {
        cycle_clock = clock;
        tasks_mark_frame_isr();
    }

    // No host answers for a while (it is off, or out of reach): the battery is not spent waiting.
    // On a cable there is nothing to save.
    battery_status_s power = {0};
    battery_get_status(&power);
    if (floss_le_link_count() || power.plugged)
    {
        _hid_ble_alone_since_us = 0;
        return;
    }
    if (!_hid_ble_alone_since_us)
        _hid_ble_alone_since_us = now_us;
    if ((now_us - _hid_ble_alone_since_us) >= HID_BLE_ALONE_OFF_US)
    {
        _hid_ble_alone_since_us = now_us;
        transport_evt_cb((tp_evt_s){
            .evt = TP_EVT_POWERCOMMAND,
            .evt_powercommand = {.power_command = TP_POWERCOMMAND_SHUTDOWN},
        });
    }
}

#endif
