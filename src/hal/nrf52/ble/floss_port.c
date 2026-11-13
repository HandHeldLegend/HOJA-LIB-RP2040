#include "hal/nrf52/ble_floss.h"

#include <string.h>

#include "floss/floss.h"
#include "hal/nrf52/ble_controller.h"
#include "hal/record_hal.h"
#include "hal/sys_hal.h"
#include "utilities/bench_log.h"

// FLOSS's side of the nRF52: its clock, random numbers, critical sections and saved records, and the
// transport that hands HCI packets to the controller in the same chip (nimble_port.c).

// The largest packet the controller sends up: an event (2 + 255) or ACL data (4 + 251)
#define FLOSS_PORT_RX_MAX_LEN   260u

// What FLOSS saves (each host's keys, its own): entries back to back, a key (two bytes), a length
// and that many bytes. The whole of it is one record in the board's store, so a host paired once is
// still paired after the power has been off, and after an update where the board carries its
// records over one.
#define FLOSS_PORT_STORE_LEN      1024u // Eight hosts' keys take about 600
#define FLOSS_PORT_STORE_HEAD_LEN 3u
#define FLOSS_PORT_STORE_ENTRY_MAX_LEN 255u
// A save stops the CPU for about 130 ms (flash_hal.c), and pairing writes several keys one after
// another: it waits for this long with nothing new
#define FLOSS_PORT_STORE_SAVE_US  1000000u

typedef enum
{
    FLOSS_PORT_STORE_CLEAN,
    FLOSS_PORT_STORE_LATER, // An entry's bytes changed: saved once no host is connected
    FLOSS_PORT_STORE_SOON,  // An entry came or went: saved though a host is connected
} floss_port_store_state_t;

static uint8_t _floss_port_store[FLOSS_PORT_STORE_LEN];
static uint16_t _floss_port_store_used = 0;
static bool _floss_port_store_loaded = false;
static floss_port_store_state_t _floss_port_store_state = FLOSS_PORT_STORE_CLEAN;
static uint64_t _floss_port_store_changed_us = 0;

// ---- What FLOSS needs from the chip (floss_hal.h) ----

uint32_t floss_hal_time_us(void)
{
    return (uint32_t)sys_hal_now_us();
}

uint32_t floss_hal_critical_enter(void)
{
    return sys_hal_irq_save();
}

void floss_hal_critical_exit(uint32_t state)
{
    sys_hal_irq_restore(state);
}

void floss_hal_random(uint8_t *buf, size_t len)
{
    while (len)
    {
        const uint32_t value = sys_hal_random();
        const size_t take = (len < sizeof(value)) ? len : sizeof(value);
        memcpy(buf, &value, take);
        buf += take;
        len -= take;
    }
}

void floss_hal_log_write(const char *text, size_t len)
{
    bench_log_write(text, len);
}

void floss_hal_debug_pin(uint8_t pin, bool level)
{
    (void)pin;
    (void)level;
}

// The entry after this one, or the end of what is used
static uint16_t _floss_port_store_next(uint16_t at)
{
    return (uint16_t)(at + FLOSS_PORT_STORE_HEAD_LEN + _floss_port_store[at + 2u]);
}

// The saved record, once. One that does not end on an entry's end is not taken.
static void _floss_port_store_load(void)
{
    if (_floss_port_store_loaded)
        return;
    _floss_port_store_loaded = true;

    uint32_t len = 0;
    const uint8_t *saved = record_hal_find(RECORD_HAL_ID_RADIO, &len);
    if (!saved || (len > FLOSS_PORT_STORE_LEN))
        return;

    memcpy(_floss_port_store, saved, len);
    uint16_t at = 0;
    while ((uint32_t)(at + FLOSS_PORT_STORE_HEAD_LEN) <= len)
        at = _floss_port_store_next(at);
    _floss_port_store_used = (at == len) ? (uint16_t)len : 0;
}

// Where a key's entry starts, or the end of what is used
static uint16_t _floss_port_store_find(uint16_t key)
{
    _floss_port_store_load();
    uint16_t at = 0;
    while (at < _floss_port_store_used)
    {
        const uint16_t entry_key = (uint16_t)(_floss_port_store[at] | (_floss_port_store[at + 1u] << 8));
        if (entry_key == key)
            break;
        at = _floss_port_store_next(at);
    }
    return at;
}

static void _floss_port_store_remove(uint16_t at)
{
    const uint16_t next = _floss_port_store_next(at);
    memmove(&_floss_port_store[at], &_floss_port_store[next], (size_t)(_floss_port_store_used - next));
    _floss_port_store_used = (uint16_t)(_floss_port_store_used - (next - at));
}

static void _floss_port_store_changed(floss_port_store_state_t state)
{
    if (state > _floss_port_store_state)
        _floss_port_store_state = state;
    _floss_port_store_changed_us = sys_hal_now_us();
}

int floss_hal_store_read(uint16_t key, void *buf, uint16_t len)
{
    const uint16_t at = _floss_port_store_find(key);
    if (at >= _floss_port_store_used)
        return FLOSS_ERR_NOT_FOUND;

    const uint16_t entry_len = _floss_port_store[at + 2u];
    const uint16_t n = (entry_len < len) ? entry_len : len;
    memcpy(buf, &_floss_port_store[at + FLOSS_PORT_STORE_HEAD_LEN], n);
    return n;
}

floss_err_t floss_hal_store_write(uint16_t key, const void *buf, uint16_t len)
{
    if (len > FLOSS_PORT_STORE_ENTRY_MAX_LEN)
        return FLOSS_ERR_INVALID;

    uint16_t at = _floss_port_store_find(key);
    const bool known = (at < _floss_port_store_used);
    const uint16_t old_len = known ? _floss_port_store[at + 2u] : 0;

    if (known && (old_len == len))
    {
        // In place. FLOSS writes a host's entry back at every connection, often as it was.
        uint8_t *data = &_floss_port_store[at + FLOSS_PORT_STORE_HEAD_LEN];
        if (memcmp(data, buf, len))
        {
            memcpy(data, buf, len);
            _floss_port_store_changed(FLOSS_PORT_STORE_LATER);
        }
        return FLOSS_OK;
    }

    const uint16_t freed = known ? (uint16_t)(FLOSS_PORT_STORE_HEAD_LEN + old_len) : 0;
    if ((uint32_t)(_floss_port_store_used - freed) + FLOSS_PORT_STORE_HEAD_LEN + len > FLOSS_PORT_STORE_LEN)
        return FLOSS_ERR_NOMEM;
    if (known)
        _floss_port_store_remove(at);

    at = _floss_port_store_used;
    _floss_port_store[at] = (uint8_t)key;
    _floss_port_store[at + 1u] = (uint8_t)(key >> 8);
    _floss_port_store[at + 2u] = (uint8_t)len;
    memcpy(&_floss_port_store[at + FLOSS_PORT_STORE_HEAD_LEN], buf, len);
    _floss_port_store_used = (uint16_t)(at + FLOSS_PORT_STORE_HEAD_LEN + len);
    _floss_port_store_changed(FLOSS_PORT_STORE_SOON);
    return FLOSS_OK;
}

floss_err_t floss_hal_store_delete(uint16_t key)
{
    const uint16_t at = _floss_port_store_find(key);
    if (at < _floss_port_store_used)
    {
        _floss_port_store_remove(at);
        _floss_port_store_changed(FLOSS_PORT_STORE_SOON);
    }
    return FLOSS_OK;
}

void nrf52_ble_floss_store_flush(void)
{
    if (_floss_port_store_state == FLOSS_PORT_STORE_CLEAN)
        return;

    const record_hal_part_s part = {.data = _floss_port_store, .len = _floss_port_store_used};
    if (record_hal_write(RECORD_HAL_ID_RADIO, &part, 1))
        _floss_port_store_state = FLOSS_PORT_STORE_CLEAN;
    else
        _floss_port_store_changed_us = sys_hal_now_us(); // Tried again after the same wait
}

void nrf52_ble_floss_store_task(void)
{
    if (_floss_port_store_state == FLOSS_PORT_STORE_CLEAN)
        return;
    if ((_floss_port_store_state == FLOSS_PORT_STORE_LATER) && floss_le_link_count())
        return;
    if ((sys_hal_now_us() - _floss_port_store_changed_us) < FLOSS_PORT_STORE_SAVE_US)
        return;
    nrf52_ble_floss_store_flush();
}

// ---- The transport ----

static floss_err_t _floss_port_open(floss_transport_t *t)
{
    (void)t;
    return nrf52_ble_controller_start() ? FLOSS_OK : FLOSS_ERR_IO;
}

static void _floss_port_close(floss_transport_t *t)
{
    (void)t;
}

static floss_err_t _floss_port_send(floss_transport_t *t, uint8_t type, uint8_t *pkt, uint16_t len)
{
    (void)t;
    // The controller copies the packet into a buffer of its own, and has none free when its host
    // is ahead of the radio
    return nrf52_ble_hci_send(type, pkt, len) ? FLOSS_OK : FLOSS_ERR_BUSY;
}

static int _floss_port_poll(floss_transport_t *t)
{
    static uint8_t packet[FLOSS_PORT_RX_MAX_LEN];

    int delivered = 0;
    uint8_t type;
    uint16_t len;
    while ((len = nrf52_ble_hci_receive(&type, packet, sizeof(packet))) != 0)
    {
        floss_transport_deliver(t, type, packet, len);
        delivered++;
    }
    return delivered;
}

static const floss_transport_ops_t _floss_port_ops = {
    .name = "nrf52",
    .headroom = 0,
    .open = _floss_port_open,
    .close = _floss_port_close,
    .send = _floss_port_send,
    .poll = _floss_port_poll,
};

static floss_transport_t _floss_port_transport = {
    .ops = &_floss_port_ops,
};

floss_transport_t *nrf52_ble_floss_transport(void)
{
    return &_floss_port_transport;
}
