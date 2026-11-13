#include "hal/nrf52/ble_floss.h"

#include <string.h>

#include "floss/floss_hal.h"
#include "hal/nrf52/ble_controller.h"
#include "hal/sys_hal.h"
#include "utilities/bench_log.h"

// FLOSS's side of the nRF52: its clock, random numbers, critical sections and saved records, and the
// transport that hands HCI packets to the controller in the same chip (nimble_port.c).

// The largest packet the controller sends up: an event (2 + 255) or ACL data (4 + 251)
#define FLOSS_PORT_RX_MAX_LEN   260u

// What FLOSS saves (bonds, its address). Kept in RAM for now: lost at power off.
#define FLOSS_PORT_STORE_SLOTS  20u
#define FLOSS_PORT_STORE_MAX_LEN 124u

typedef struct
{
    uint16_t key;
    uint8_t len;
    bool used;
    uint8_t data[FLOSS_PORT_STORE_MAX_LEN];
} floss_port_record_s;

static floss_port_record_s _floss_port_store[FLOSS_PORT_STORE_SLOTS];

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

static floss_port_record_s *_floss_port_store_find(uint16_t key)
{
    for (uint8_t i = 0; i < FLOSS_PORT_STORE_SLOTS; i++)
    {
        if (_floss_port_store[i].used && (_floss_port_store[i].key == key))
            return &_floss_port_store[i];
    }
    return NULL;
}

int floss_hal_store_read(uint16_t key, void *buf, uint16_t len)
{
    const floss_port_record_s *record = _floss_port_store_find(key);
    if (!record)
        return FLOSS_ERR_NOT_FOUND;

    const uint16_t n = (record->len < len) ? record->len : len;
    memcpy(buf, record->data, n);
    return n;
}

floss_err_t floss_hal_store_write(uint16_t key, const void *buf, uint16_t len)
{
    if (len > FLOSS_PORT_STORE_MAX_LEN)
        return FLOSS_ERR_INVALID;

    floss_port_record_s *record = _floss_port_store_find(key);
    for (uint8_t i = 0; !record && (i < FLOSS_PORT_STORE_SLOTS); i++)
    {
        if (!_floss_port_store[i].used)
            record = &_floss_port_store[i];
    }
    if (!record)
        return FLOSS_ERR_NOMEM;

    record->key = key;
    record->len = (uint8_t)len;
    record->used = true;
    memcpy(record->data, buf, len);
    return FLOSS_OK;
}

floss_err_t floss_hal_store_delete(uint16_t key)
{
    floss_port_record_s *record = _floss_port_store_find(key);
    if (record)
        record->used = false;
    return FLOSS_OK;
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
