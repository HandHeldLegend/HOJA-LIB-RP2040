#include "board_config.h"

// The config app over Bluetooth LE (see ble_config.h). The app speaks the same 64-byte packets as
// over WebUSB, wrapped in [type][payload] frames on two characteristics: one the app writes, one
// the gamepad notifies.
#if defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_FLOSS
#include "hal/rp2040/ble_config.h"

#include <stdio.h>
#include <string.h>

#include "floss/floss.h"

#include "hal/sys_hal.h"
#include "input/hover.h"
#include "input/mapper.h"
#include "input_shared_types.h"
#include "usb/webusb.h"
#include "utilities/static_config.h"
#include "utilities/settings.h"
#include "hoja.h"

// Plus + Minus opens the beacon for this long: time to open the app, pick the gamepad in the
// browser's list and connect (a wired mode first brings the radio up)
#define BLE_CFG_BEACON_US   30000000u
#define BLE_CFG_ADV_FAST    0x00A0u // 100 ms
#define BLE_CFG_ADV_SLOW    0x0190u // 250 ms, beside a link that fills the air
// The code: six presses, three face buttons and three d-pad directions, within this long
#define BLE_CFG_CODE_LEN    6u
#define BLE_CFG_CODE_US     30000000u
// Browsers allowed before, oldest first (one value in the pairing store)
#define BLE_CFG_TOKEN_LEN   16u
#define BLE_CFG_TOKENS      4u
#define BLE_CFG_STORE_KEY   0x8001u
#define BLE_CFG_RX_LEN      8u
#define BLE_CFG_FRAME_MAX   (1u + 64u)
// A big config block fills the notification buffers; keep the radio going until they drain
#define BLE_CFG_SEND_WAIT_US 200000u
// An app that hasn't written for this long (it pings every 2 s) is gone
#define BLE_CFG_QUIET_US     5000000u

typedef enum
{
    BLE_CFG_TO_PAD_AUTH = 0x01,       // [token 16]
    BLE_CFG_TO_PAD_PAIR = 0x02,       // Ask for a code
    BLE_CFG_TO_PAD_DATA = 0x03,       // [WebUSB packet]
    BLE_CFG_TO_PAD_BYE = 0x04,        // The app is done: the gamepad closes the link
    BLE_CFG_TO_PAD_PING = 0x05,       // The app is still there (every 2 s while connected)
    BLE_CFG_TO_APP_AUTH = 0x81,       // [0 allowed, 1 refused]
    BLE_CFG_TO_APP_CODE = 0x82,       // [six letters: A B X Y as printed, U D L R for the d-pad]
    BLE_CFG_TO_APP_CODE_STATE = 0x83, // [presses entered][0 entering, 1 wrong (a new code follows), 2 timed out]
    BLE_CFG_TO_APP_PAIRED = 0x84,     // [token 16]: allowed from now on
    BLE_CFG_TO_APP_DATA = 0x85,       // [WebUSB packet]
    BLE_CFG_TO_APP_INPUT = 0x86,      // [live input report]
    BLE_CFG_TO_APP_LINK = 0x87,       // [interval, latency, ATT MTU, LL tx octets: u16 LE][tx PHY][rx PHY]
} ble_cfg_frame_t;

typedef enum
{
    BLE_CFG_CODE_STATE_ENTERING = 0,
    BLE_CFG_CODE_STATE_WRONG,
    BLE_CFG_CODE_STATE_TIMEOUT,
} ble_cfg_code_state_t;

// b4c1f0a0-3e6d-4c5a-9f2e-6d1a7c8b9e10 (service), ...9e11 (app writes), ...9e12 (gamepad notifies);
// little-endian, as on the wire
static const uint8_t _ble_cfg_uuid[16] = {0x10, 0x9E, 0x8B, 0x7C, 0x1A, 0x6D, 0x2E, 0x9F,
                                          0x5A, 0x4C, 0x6D, 0x3E, 0xA0, 0xF0, 0xC1, 0xB4};
static const uint8_t _ble_cfg_rx_uuid[16] = {0x11, 0x9E, 0x8B, 0x7C, 0x1A, 0x6D, 0x2E, 0x9F,
                                             0x5A, 0x4C, 0x6D, 0x3E, 0xA0, 0xF0, 0xC1, 0xB4};
static const uint8_t _ble_cfg_tx_uuid[16] = {0x12, 0x9E, 0x8B, 0x7C, 0x1A, 0x6D, 0x2E, 0x9F,
                                             0x5A, 0x4C, 0x6D, 0x3E, 0xA0, 0xF0, 0xC1, 0xB4};

// The buttons a code is made of, with the letters the app shows (face buttons as printed)
static const int8_t _ble_cfg_face[4] = {INPUT_CODE_SOUTH, INPUT_CODE_EAST, INPUT_CODE_WEST, INPUT_CODE_NORTH};
static const int8_t _ble_cfg_dpad[4] = {INPUT_CODE_UP, INPUT_CODE_DOWN, INPUT_CODE_LEFT, INPUT_CODE_RIGHT};
static const char _ble_cfg_dpad_letter[4] = {'U', 'D', 'L', 'R'};

typedef struct
{
    uint8_t len;
    uint8_t data[64];
} ble_cfg_rx_s;

static const char *_ble_cfg_name = "HOJA";
static bool _ble_cfg_slow = false;
static uint64_t _ble_cfg_beacon_until = 0; // 0: no beacon
static bool _ble_cfg_beacon_pending = false; // Plus + Minus asked for it while a link was open
static bool _ble_cfg_close = false;          // Close the app's link (from the task, after the write's answer)
static volatile uint64_t _ble_cfg_heard_us = 0; // When the app last wrote anything
static bool _ble_cfg_chord = false;        // Plus + Minus were down at the last look

static uint16_t _ble_cfg_conn = 0xFFFFu;   // The app's LE link
static bool _ble_cfg_allowed = false;
static const webusb_sink_s *_ble_cfg_prev_sink = NULL;

static bool _ble_cfg_code_wanted = false; // The app asked for a code (sent from the task)
static bool _ble_cfg_entering = false;
static uint64_t _ble_cfg_code_until = 0;
static int8_t _ble_cfg_code[BLE_CFG_CODE_LEN];
static char _ble_cfg_code_letter[BLE_CFG_CODE_LEN];
static uint8_t _ble_cfg_code_pos = 0;
static uint8_t _ble_cfg_held = 0; // Code buttons down at the last look (bit per _face, then _dpad)

static ble_cfg_rx_s _ble_cfg_rx[BLE_CFG_RX_LEN];
static uint8_t _ble_cfg_rx_head = 0;
static uint8_t _ble_cfg_rx_tail = 0;

static floss_gatt_cccd_t _ble_cfg_tx_cccd;
static uint16_t _ble_cfg_tx_handle = 0;

static uint8_t _ble_cfg_rx_write(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr,
                                 uint16_t offset, const uint8_t *data, uint16_t len, uint8_t flags);

static const floss_gatt_attr_t _ble_cfg_service[] = {
    FLOSS_GATT_PRIMARY_SERVICE128(_ble_cfg_uuid),
    FLOSS_GATT_CHARACTERISTIC(FLOSS_GATT_PROP_WRITE | FLOSS_GATT_PROP_WRITE_NO_RSP),
    {.uuid128 = _ble_cfg_rx_uuid, .perms = FLOSS_GATT_P_WRITE, .write = _ble_cfg_rx_write,
     .max_len = BLE_CFG_FRAME_MAX},
    FLOSS_GATT_CHARACTERISTIC(FLOSS_GATT_PROP_NOTIFY),
    {.uuid128 = _ble_cfg_tx_uuid, .perms = 0, .max_len = BLE_CFG_FRAME_MAX},
    FLOSS_GATT_CCCD(_ble_cfg_tx_cccd),
};

/***********************************************/
/********* Sending *****************************/

static floss_err_t _ble_cfg_notify(uint8_t type, const uint8_t *data, uint16_t len)
{
    uint8_t frame[BLE_CFG_FRAME_MAX];
    if (_ble_cfg_conn == 0xFFFFu || len > sizeof(frame) - 1u)
        return FLOSS_ERR_STATE;
    frame[0] = type;
    if (len)
        memcpy(&frame[1], data, len);
    return floss_gatt_notify(_ble_cfg_conn, _ble_cfg_tx_handle, frame, (uint16_t)(len + 1u));
}

// Replies must all arrive: wait for buffers, polling the stack so they drain
static bool _ble_cfg_notify_wait(uint8_t type, const uint8_t *data, uint16_t len)
{
    uint64_t until = sys_hal_now_us() + BLE_CFG_SEND_WAIT_US;
    for (;;)
    {
        floss_err_t err = _ble_cfg_notify(type, data, len);
        if (err == FLOSS_OK)
            return true;
        if (err != FLOSS_ERR_BUSY || sys_hal_now_us() > until)
            return false;
        floss_poll();
    }
}

static bool _ble_cfg_sink_send(const uint8_t *data, uint16_t size)
{
    return _ble_cfg_notify_wait(BLE_CFG_TO_APP_DATA, data, size);
}

// The live input report: the newest one wins, so it goes only when nothing else waits on the link.
// A slow link (the ESP32 empties its LE queue slower than 125 reports a second) would otherwise fill
// the queue with input nobody wants any more, and the app's replies would wait behind it.
static bool _ble_cfg_sink_send_input(const uint8_t *data, uint16_t size)
{
    if (_ble_cfg_conn != 0xFFFFu && floss_hci_acl_can_send_fresh(_ble_cfg_conn, 1))
        _ble_cfg_notify(BLE_CFG_TO_APP_INPUT, data, size);
    return true;
}

static bool _ble_cfg_sink_ready(int timeout_ms)
{
    (void)timeout_ms;
    return _ble_cfg_allowed;
}

static const webusb_sink_s _ble_cfg_sink = {
    .send = _ble_cfg_sink_send,
    .send_input = _ble_cfg_sink_send_input,
    .ready = _ble_cfg_sink_ready,
};

/***********************************************/
/********* Allowed browsers ********************/

static void _ble_cfg_tokens_load(uint8_t tokens[BLE_CFG_TOKENS][BLE_CFG_TOKEN_LEN])
{
    if (floss_hal_store_read(BLE_CFG_STORE_KEY, tokens, BLE_CFG_TOKENS * BLE_CFG_TOKEN_LEN) !=
        (int)(BLE_CFG_TOKENS * BLE_CFG_TOKEN_LEN))
        memset(tokens, 0, BLE_CFG_TOKENS * BLE_CFG_TOKEN_LEN);
}

static bool _ble_cfg_token_known(const uint8_t *token)
{
    static const uint8_t blank[BLE_CFG_TOKEN_LEN] = {0};
    if (!memcmp(token, blank, BLE_CFG_TOKEN_LEN))
        return false;

    uint8_t tokens[BLE_CFG_TOKENS][BLE_CFG_TOKEN_LEN];
    _ble_cfg_tokens_load(tokens);
    for (uint8_t i = 0; i < BLE_CFG_TOKENS; i++)
    {
        if (!memcmp(tokens[i], token, BLE_CFG_TOKEN_LEN))
            return true;
    }
    return false;
}

// A new browser: the oldest one makes room
static void _ble_cfg_token_add(const uint8_t *token)
{
    uint8_t tokens[BLE_CFG_TOKENS][BLE_CFG_TOKEN_LEN];
    _ble_cfg_tokens_load(tokens);
    memmove(tokens[0], tokens[1], (BLE_CFG_TOKENS - 1u) * BLE_CFG_TOKEN_LEN);
    memcpy(tokens[BLE_CFG_TOKENS - 1u], token, BLE_CFG_TOKEN_LEN);
    floss_hal_store_write(BLE_CFG_STORE_KEY, tokens, sizeof(tokens));
}

// What the LE link runs with, for the app (and bench work): changes as the browser and the
// controller settle the parameters, data length and PHY after connecting
static void _ble_cfg_link_report(void)
{
    const floss_le_link_t *l = floss_le_link(_ble_cfg_conn);
    if (!l || !_ble_cfg_allowed)
        return;
    uint8_t msg[10];
    floss_put_le16(&msg[0], l->interval);
    floss_put_le16(&msg[2], l->latency);
    floss_put_le16(&msg[4], l->att_mtu);
    floss_put_le16(&msg[6], l->tx_octets);
    msg[8] = l->tx_phy;
    msg[9] = l->rx_phy;
    _ble_cfg_notify(BLE_CFG_TO_APP_LINK, msg, sizeof(msg));
}

static void _ble_cfg_allow(bool allowed)
{
    if (allowed == _ble_cfg_allowed)
        return;
    _ble_cfg_allowed = allowed;
    if (allowed)
    {
        // The app's traffic comes here until it disconnects, then goes back (USB, or the dongle)
        _ble_cfg_prev_sink = webusb_get_sink();
        webusb_set_sink(&_ble_cfg_sink);
        _ble_cfg_link_report();
    }
    else
    {
        webusb_set_sink(_ble_cfg_prev_sink);
        _ble_cfg_prev_sink = NULL;
    }
}

/***********************************************/
/********* The code ****************************/

static uint32_t _ble_cfg_random(uint32_t below)
{
    return sys_hal_random() % below;
}

// Three face buttons and three d-pad presses, in a random order
static void _ble_cfg_code_new(void)
{
    bool face[BLE_CFG_CODE_LEN] = {true, true, true, false, false, false};
    for (uint8_t i = BLE_CFG_CODE_LEN - 1u; i > 0; i--)
    {
        uint8_t j = (uint8_t)_ble_cfg_random(i + 1u);
        bool t = face[i];
        face[i] = face[j];
        face[j] = t;
    }

    for (uint8_t i = 0; i < BLE_CFG_CODE_LEN; i++)
    {
        uint8_t pick = (uint8_t)_ble_cfg_random(4u);
        if (face[i])
        {
            _ble_cfg_code[i] = _ble_cfg_face[pick];
            char letter = (char)input_static.input_info[_ble_cfg_face[pick]].input_name[0];
            _ble_cfg_code_letter[i] = letter ? letter : '?';
        }
        else
        {
            _ble_cfg_code[i] = _ble_cfg_dpad[pick];
            _ble_cfg_code_letter[i] = _ble_cfg_dpad_letter[pick];
        }
    }
    _ble_cfg_code_pos = 0;
    _ble_cfg_code_until = sys_hal_now_us() + BLE_CFG_CODE_US;
}

static void _ble_cfg_code_start(void)
{
    _ble_cfg_code_new();
    _ble_cfg_entering = true;
    _ble_cfg_held = 0xFF; // Buttons already down don't count until released
    mapper_hold_neutral(true);
    _ble_cfg_notify_wait(BLE_CFG_TO_APP_CODE, (const uint8_t *)_ble_cfg_code_letter, BLE_CFG_CODE_LEN);
}

static void _ble_cfg_code_stop(void)
{
    if (!_ble_cfg_entering)
        return;
    _ble_cfg_entering = false;
    mapper_hold_neutral(false);
}

static void _ble_cfg_code_state(ble_cfg_code_state_t state)
{
    uint8_t msg[2] = {_ble_cfg_code_pos, (uint8_t)state};
    _ble_cfg_notify_wait(BLE_CFG_TO_APP_CODE_STATE, msg, sizeof(msg));
}

// The code buttons pressed since the last look, one bit each (face buttons, then the d-pad)
static uint8_t _ble_cfg_code_presses(const mapper_input_s *in)
{
    uint8_t down = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (in->presses[_ble_cfg_face[i]])
            down |= (uint8_t)(1u << i);
        if (in->presses[_ble_cfg_dpad[i]])
            down |= (uint8_t)(1u << (4 + i));
    }
    uint8_t pressed = down & (uint8_t)~_ble_cfg_held;
    _ble_cfg_held = down;
    return pressed;
}

static int8_t _ble_cfg_bit_code(uint8_t bit)
{
    return (bit < 4) ? _ble_cfg_face[bit] : _ble_cfg_dpad[bit - 4];
}

static void _ble_cfg_code_task(uint64_t now_us, const mapper_input_s *in)
{
    if (!_ble_cfg_entering)
        return;

    if (now_us > _ble_cfg_code_until)
    {
        _ble_cfg_code_state(BLE_CFG_CODE_STATE_TIMEOUT);
        _ble_cfg_code_stop();
        return;
    }

    uint8_t pressed = _ble_cfg_code_presses(in);
    for (uint8_t bit = 0; pressed && bit < 8; bit++)
    {
        if (!(pressed & (1u << bit)))
            continue;
        pressed &= (uint8_t)~(1u << bit);

        if (_ble_cfg_bit_code(bit) != _ble_cfg_code[_ble_cfg_code_pos])
        {
            // Wrong: a fresh code, so guessing gains nothing
            _ble_cfg_code_state(BLE_CFG_CODE_STATE_WRONG);
            _ble_cfg_code_new();
            _ble_cfg_notify_wait(BLE_CFG_TO_APP_CODE, (const uint8_t *)_ble_cfg_code_letter, BLE_CFG_CODE_LEN);
            return;
        }

        _ble_cfg_code_pos++;
        if (_ble_cfg_code_pos < BLE_CFG_CODE_LEN)
        {
            _ble_cfg_code_state(BLE_CFG_CODE_STATE_ENTERING);
            continue;
        }

        // Entered: this browser is allowed from now on
        uint8_t token[BLE_CFG_TOKEN_LEN];
        for (uint8_t i = 0; i < BLE_CFG_TOKEN_LEN; i += 4)
        {
            uint32_t r = sys_hal_random();
            memcpy(&token[i], &r, 4);
        }
        _ble_cfg_token_add(token);
        _ble_cfg_code_stop();
        _ble_cfg_notify_wait(BLE_CFG_TO_APP_PAIRED, token, sizeof(token));
        _ble_cfg_allow(true);
        return;
    }
}

/***********************************************/
/********* GATT and LE *************************/

static uint8_t _ble_cfg_rx_write(uint16_t conn, uint16_t attr_handle, const floss_gatt_attr_t *attr,
                                 uint16_t offset, const uint8_t *data, uint16_t len, uint8_t flags)
{
    (void)attr_handle;
    (void)attr;
    (void)flags;
    if (conn != _ble_cfg_conn || offset != 0 || len < 1)
        return FLOSS_ATT_ERR_UNLIKELY;

    // Every answer is a notification, so an app that writes wants them. A browser that kept its
    // subscription from an earlier link may not write the CCCD again on this one (it is cleared
    // when a link drops), and would then never hear back.
    int link = floss_le_link_index(conn);
    if (link >= 0)
        _ble_cfg_tx_cccd.v[link] |= FLOSS_GATT_CCCD_NOTIFY;
    _ble_cfg_heard_us = sys_hal_now_us();

    switch (data[0])
    {
    case BLE_CFG_TO_PAD_AUTH:
    {
        if (len != 1u + BLE_CFG_TOKEN_LEN)
            return FLOSS_ATT_ERR_INVALID_VALUE_LEN;
        bool known = _ble_cfg_token_known(&data[1]);
        uint8_t result = known ? 0 : 1;
        _ble_cfg_notify(BLE_CFG_TO_APP_AUTH, &result, 1);
        if (known)
            _ble_cfg_allow(true);
        return 0;
    }

    case BLE_CFG_TO_PAD_PAIR:
        // Started from the task: the code goes out as a notification after this write's answer
        if (!_ble_cfg_allowed && !_ble_cfg_entering)
            _ble_cfg_code_wanted = true;
        return 0;

    case BLE_CFG_TO_PAD_BYE:
        _ble_cfg_close = true;
        return 0;

    case BLE_CFG_TO_PAD_PING:
        return 0;

    case BLE_CFG_TO_PAD_DATA:
    {
        if (!_ble_cfg_allowed)
            return FLOSS_ATT_ERR_WRITE_NOT_PERMITTED;
        uint8_t next = (uint8_t)((_ble_cfg_rx_head + 1u) % BLE_CFG_RX_LEN);
        if (next == _ble_cfg_rx_tail || len > 1u + 64u)
            return FLOSS_ATT_ERR_INSUFFICIENT_RESOURCES;
        _ble_cfg_rx[_ble_cfg_rx_head].len = (uint8_t)(len - 1u);
        memcpy(_ble_cfg_rx[_ble_cfg_rx_head].data, &data[1], len - 1u);
        _ble_cfg_rx_head = next;
        return 0;
    }

    default:
        return FLOSS_ATT_ERR_REQUEST_NOT_SUPPORTED;
    }
}

static void _ble_cfg_le_event(const floss_le_evt_t *evt, void *user)
{
    (void)user;
    switch (evt->type)
    {
    case FLOSS_LE_EVT_CONNECTED:
        _ble_cfg_conn = evt->handle;
        _ble_cfg_heard_us = sys_hal_now_us();
        _ble_cfg_beacon_until = 0;
        _ble_cfg_rx_head = _ble_cfg_rx_tail = 0;
        break;

    case FLOSS_LE_EVT_CONN_PARAMS:
    case FLOSS_LE_EVT_MTU:
    case FLOSS_LE_EVT_DATA_LENGTH:
    case FLOSS_LE_EVT_PHY:
        if (evt->handle == _ble_cfg_conn)
            _ble_cfg_link_report();
        break;

    case FLOSS_LE_EVT_DISCONNECTED:
        if (evt->handle != _ble_cfg_conn)
            break;
        _ble_cfg_conn = 0xFFFFu;
        _ble_cfg_code_wanted = false;
        _ble_cfg_close = false;
        _ble_cfg_code_stop();
        _ble_cfg_allow(false);
        break;

    default:
        break;
    }
}

// The name the browser's device list shows: the model, and the user's name from the config app
// when set ("GCU 2 (Mitch)"), not the mode's HID name (Switch mode would say "Pro Controller")
static void _ble_cfg_beacon_name(char *out, size_t cap)
{
    const hoja_config_s *cfg = hoja_config_get();
    const char *model = (cfg && cfg->device_name) ? cfg->device_name : _ble_cfg_name;
    char user[sizeof(user_config->user_name) + 1] = {0};
    memcpy(user, user_config->user_name, sizeof(user_config->user_name));
    if (user[0])
        snprintf(out, cap, "%s (%s)", model, user);
    else
        snprintf(out, cap, "%s", model);
}

static void _ble_cfg_beacon_start(uint64_t now_us)
{
    char name[30];
    _ble_cfg_beacon_name(name, sizeof(name));

    floss_le_adv_params_t p;
    floss_le_adv_params_defaults(&p);
    p.interval_min = _ble_cfg_slow ? BLE_CFG_ADV_SLOW : BLE_CFG_ADV_FAST;
    p.interval_max = p.interval_min;
    // An LE-only identity of its own: Windows would otherwise mix this link up with the gamepad's
    // Classic pairing, which shares the public address
    p.own_addr_type = FLOSS_LE_ADDR_RANDOM;
    p.auto_restart = false;
    floss_le_adv_set_params(&p);

    floss_le_ad_t ad;
    floss_le_ad_init(&ad);
    floss_le_ad_flags(&ad, FLOSS_AD_FLAG_GENERAL_DISC | FLOSS_AD_FLAG_NO_BREDR);
    floss_le_ad_add(&ad, FLOSS_AD_UUID128_ALL, _ble_cfg_uuid, sizeof(_ble_cfg_uuid));
    if (FLOSS_LE_ADV_DATA_MAX - ad.len >= 3)
        floss_le_ad_name(&ad, name);
    floss_le_adv_set_data(ad.data, ad.len);

    floss_le_ad_t rsp;
    floss_le_ad_init(&rsp);
    floss_le_ad_name(&rsp, name);
    floss_le_adv_set_scan_rsp(rsp.data, rsp.len);

    floss_le_adv_start();
    _ble_cfg_beacon_until = now_us + BLE_CFG_BEACON_US;
}

void ble_config_setup(const char *name, bool slow_beacon)
{
    // A restart drops every link without callbacks
    _ble_cfg_conn = 0xFFFFu;
    _ble_cfg_beacon_until = 0;
    _ble_cfg_beacon_pending = false;
    _ble_cfg_close = false;
    _ble_cfg_code_wanted = false;
    _ble_cfg_code_stop();
    _ble_cfg_allow(false);

    _ble_cfg_name = name ? name : "HOJA";
    _ble_cfg_slow = slow_beacon;
    uint16_t base = floss_gatt_register(_ble_cfg_service, FLOSS_ARRAY_SIZE(_ble_cfg_service));
    _ble_cfg_tx_handle = base ? (uint16_t)(base + 4u) : 0;
    floss_le_add_listener(_ble_cfg_le_event, NULL);
}

void ble_config_task(uint64_t now_us)
{
    mapper_input_s in = {0};
    hover_access_safe(&in);

    // Plus and Minus pressed together open the beacon (again). An app still talking keeps its
    // session. A link gone quiet (a browser closed its side but the computer kept the link, or a tab
    // crashed) is closed first, so the press starts afresh.
    bool chord = in.presses[INPUT_CODE_START] && in.presses[INPUT_CODE_SELECT];
    if (chord && !_ble_cfg_chord && _ble_cfg_tx_handle)
    {
        if (_ble_cfg_conn == 0xFFFFu)
            _ble_cfg_beacon_start(now_us);
        else if (!_ble_cfg_allowed || (now_us - _ble_cfg_heard_us) > BLE_CFG_QUIET_US)
        {
            _ble_cfg_beacon_pending = true;
            _ble_cfg_close = true;
        }
    }
    _ble_cfg_chord = chord;

    if (_ble_cfg_close && _ble_cfg_conn != 0xFFFFu)
    {
        _ble_cfg_close = false;
        floss_le_disconnect(_ble_cfg_conn, FLOSS_HCI_ERR_REMOTE_USER_TERMINATED);
    }
    if (_ble_cfg_beacon_pending && _ble_cfg_conn == 0xFFFFu)
    {
        _ble_cfg_beacon_pending = false;
        _ble_cfg_beacon_start(now_us);
    }

    if (_ble_cfg_beacon_until && now_us > _ble_cfg_beacon_until)
    {
        _ble_cfg_beacon_until = 0;
        if (_ble_cfg_conn == 0xFFFFu)
            floss_le_adv_stop();
    }

    // A code was asked for (from the write callback)
    if (_ble_cfg_code_wanted)
    {
        _ble_cfg_code_wanted = false;
        if (!_ble_cfg_allowed && !_ble_cfg_entering)
            _ble_cfg_code_start();
    }
    _ble_cfg_code_task(now_us, &in);

    if (!_ble_cfg_allowed)
        return;

    while (_ble_cfg_rx_tail != _ble_cfg_rx_head)
    {
        ble_cfg_rx_s *rx = &_ble_cfg_rx[_ble_cfg_rx_tail];
        webusb_command_handler(rx->data, rx->len);
        _ble_cfg_rx_tail = (uint8_t)((_ble_cfg_rx_tail + 1u) % BLE_CFG_RX_LEN);
    }
    webusb_send_rawinput(now_us);
}

bool ble_config_connected(void)
{
    return _ble_cfg_conn != 0xFFFFu;
}

void ble_config_open_beacon(void)
{
    _ble_cfg_beacon_pending = true;
    _ble_cfg_chord = true; // The press that asked is still down: don't count it again
}

bool ble_config_active(void)
{
    return _ble_cfg_beacon_until || _ble_cfg_beacon_pending || (_ble_cfg_conn != 0xFFFFu);
}

#endif
