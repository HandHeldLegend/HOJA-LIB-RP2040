#include "hal/nrf52/ble_controller.h"

#include <stddef.h>
#include <string.h>

#include "os/os.h"
#include "os/os_cputime.h"
#include "nimble/nimble_npl.h"
#include "nimble/transport.h"
#include "controller/ble_ll.h"
#include "controller/ble_phy.h"
#include "hal/hal_timer.h"
// For what NimBLE's link layer task sets before its loop (see nrf52_ble_controller_start)
#include "ble_ll_priv.h"

#include "hal/sys_hal.h"
#include "nrf52_hal.h"

// NimBLE's controller (its link layer and nRF radio driver) with no operating system under it.
//
// NimBLE splits the link layer in two. Everything with a deadline runs in interrupts: the radio's,
// and the timer that starts each radio event. Everything else (packets received, HCI commands,
// control procedures) is queued as events for a task. Here that task is a low-priority interrupt
// that runs whenever an event is queued, so the link layer never waits on the main loop, and the
// main loop can stall (a flash write aside: that stops the whole CPU) without dropping the link.
//
// The host (FLOSS) is in the main loop and talks HCI through the calls at the end of this file:
// commands and data go to the controller's queues, and what the controller sends back waits in a
// ring until the host polls for it.

// Interrupt priorities, most urgent first. The radio's (0) is set by NimBLE's driver.
#define NIMBLE_PORT_PRIO_CPUTIME    2u // Starts radio events on time
#define NIMBLE_PORT_PRIO_RNG        5u // Refills the random pool the task draws on
#define NIMBLE_PORT_PRIO_TASK       6u // The link layer's events
#define NIMBLE_PORT_TASK_IRQ        SWI5_EGU5_IRQn

#define NIMBLE_PORT_EVENTQ_MAX      4u
#define NIMBLE_PORT_LFCLK_WAIT_US   10000u

// Controller-to-host packets: [type][length low][length high][packet]
#define NIMBLE_PORT_RX_RING_LEN     2048u
#define NIMBLE_PORT_RX_HEADER_LEN   3u

static struct ble_npl_eventq *_nimble_eventqs[NIMBLE_PORT_EVENTQ_MAX];
static uint8_t _nimble_eventq_count = 0;
static struct ble_npl_callout *_nimble_callouts = NULL; // Active, soonest first

static void (*_nimble_isr_radio)(void) = NULL;
static void (*_nimble_isr_rtc0)(void) = NULL;
static void (*_nimble_isr_rng)(void) = NULL;
static void (*_nimble_isr_ccm_aar)(void) = NULL;

static uint8_t _nimble_rx_ring[NIMBLE_PORT_RX_RING_LEN];
static volatile uint16_t _nimble_rx_head = 0; // Written by the controller
static volatile uint16_t _nimble_rx_tail = 0; // Read by the host
static volatile uint32_t _nimble_rx_dropped = 0;

static bool _nimble_started = false;

// ---- Critical sections, time ----

uint32_t ble_npl_hw_enter_critical(void)
{
    return sys_hal_irq_save();
}

void ble_npl_hw_exit_critical(uint32_t ctx)
{
    sys_hal_irq_restore(ctx);
}

bool ble_npl_hw_is_in_critical(void)
{
    return __get_PRIMASK() != 0;
}

bool ble_npl_os_started(void)
{
    return true;
}

void *ble_npl_get_current_task_id(void)
{
    return NULL;
}

ble_npl_time_t ble_npl_time_get(void)
{
    return (ble_npl_time_t)(sys_hal_now_us() / 1000u);
}

ble_npl_error_t ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks)
{
    *out_ticks = ms;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms)
{
    *out_ms = ticks;
    return BLE_NPL_OK;
}

ble_npl_time_t ble_npl_time_ms_to_ticks32(uint32_t ms)
{
    return ms;
}

uint32_t ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks)
{
    return ticks;
}

void ble_npl_time_delay(ble_npl_time_t ticks)
{
    sys_hal_sleep_ms(ticks);
}

// ---- Events ----

void ble_npl_eventq_init(struct ble_npl_eventq *evq)
{
    evq->head = NULL;
    evq->tail = NULL;

    for (uint8_t i = 0; i < _nimble_eventq_count; i++)
    {
        if (_nimble_eventqs[i] == evq)
            return;
    }
    assert(_nimble_eventq_count < NIMBLE_PORT_EVENTQ_MAX);
    _nimble_eventqs[_nimble_eventq_count++] = evq;
}

// Never waits: the task interrupt runs what is queued and returns
struct ble_npl_event *ble_npl_eventq_get(struct ble_npl_eventq *evq, ble_npl_time_t tmo)
{
    (void)tmo;

    const uint32_t state = sys_hal_irq_save();
    struct ble_npl_event *ev = evq->head;
    if (ev)
    {
        evq->head = ev->next;
        if (!evq->head)
            evq->tail = NULL;
        ev->next = NULL;
        ev->queued = false;
    }
    sys_hal_irq_restore(state);
    return ev;
}

void ble_npl_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev)
{
    const uint32_t state = sys_hal_irq_save();
    if (!ev->queued)
    {
        ev->queued = true;
        ev->next = NULL;
        if (evq->tail)
            evq->tail->next = ev;
        else
            evq->head = ev;
        evq->tail = ev;
    }
    sys_hal_irq_restore(state);

    NVIC_SetPendingIRQ(NIMBLE_PORT_TASK_IRQ);
}

void ble_npl_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev)
{
    const uint32_t state = sys_hal_irq_save();
    if (ev->queued)
    {
        struct ble_npl_event *prev = NULL;
        for (struct ble_npl_event *at = evq->head; at; prev = at, at = at->next)
        {
            if (at != ev)
                continue;
            if (prev)
                prev->next = at->next;
            else
                evq->head = at->next;
            if (evq->tail == at)
                evq->tail = prev;
            break;
        }
        ev->next = NULL;
        ev->queued = false;
    }
    sys_hal_irq_restore(state);
}

bool ble_npl_eventq_is_empty(struct ble_npl_eventq *evq)
{
    return evq->head == NULL;
}

void ble_npl_event_init(struct ble_npl_event *ev, ble_npl_event_fn *fn, void *arg)
{
    memset(ev, 0, sizeof(*ev));
    ev->fn = fn;
    ev->arg = arg;
}

bool ble_npl_event_is_queued(struct ble_npl_event *ev)
{
    return ev->queued;
}

void *ble_npl_event_get_arg(struct ble_npl_event *ev)
{
    return ev->arg;
}

void ble_npl_event_set_arg(struct ble_npl_event *ev, void *arg)
{
    ev->arg = arg;
}

void ble_npl_event_run(struct ble_npl_event *ev)
{
    ev->fn(ev);
}

// The link layer's task: every queued event, in turn
void SWI5_EGU5_IRQHandler(void)
{
    bool ran;
    do
    {
        ran = false;
        for (uint8_t i = 0; i < _nimble_eventq_count; i++)
        {
            struct ble_npl_event *ev = ble_npl_eventq_get(_nimble_eventqs[i], 0);
            if (ev)
            {
                ble_npl_event_run(ev);
                ran = true;
            }
        }
    } while (ran);
}

// ---- Mutexes and semaphores: one task, so nothing ever waits ----

ble_npl_error_t ble_npl_mutex_init(struct ble_npl_mutex *mu)
{
    (void)mu;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout)
{
    (void)mu;
    (void)timeout;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_release(struct ble_npl_mutex *mu)
{
    (void)mu;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens)
{
    sem->count = tokens;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout)
{
    (void)timeout;

    ble_npl_error_t result = BLE_NPL_TIMEOUT;
    const uint32_t state = sys_hal_irq_save();
    if (sem->count)
    {
        sem->count--;
        result = BLE_NPL_OK;
    }
    sys_hal_irq_restore(state);
    return result;
}

ble_npl_error_t ble_npl_sem_release(struct ble_npl_sem *sem)
{
    const uint32_t state = sys_hal_irq_save();
    sem->count++;
    sys_hal_irq_restore(state);
    return BLE_NPL_OK;
}

uint16_t ble_npl_sem_get_count(struct ble_npl_sem *sem)
{
    return sem->count;
}

// ---- Callouts: one-shot timers that queue an event, on the HAL's alarm ----

static void _nimble_callout_expired(void);

// The alarm for the soonest callout. Interrupts off.
static void _nimble_callout_arm(void)
{
    if (!_nimble_callouts)
    {
        nrf52_alarm_cancel();
        return;
    }

    const int32_t in_ms = (int32_t)(_nimble_callouts->expiry - ble_npl_time_get());
    nrf52_alarm_set(sys_hal_now_us() + (uint64_t)((in_ms > 0) ? in_ms : 0) * 1000u, _nimble_callout_expired);
}

// Interrupts off
static void _nimble_callout_unlink(struct ble_npl_callout *co)
{
    struct ble_npl_callout **at = &_nimble_callouts;
    while (*at && (*at != co))
        at = &(*at)->next;
    if (*at)
        *at = co->next;
    co->next = NULL;
    co->active = false;
}

// From the alarm (the clock's interrupt)
static void _nimble_callout_expired(void)
{
    const uint32_t state = sys_hal_irq_save();
    const ble_npl_time_t now = ble_npl_time_get();

    while (_nimble_callouts && ((int32_t)(_nimble_callouts->expiry - now) <= 0))
    {
        struct ble_npl_callout *co = _nimble_callouts;
        _nimble_callouts = co->next;
        co->next = NULL;
        co->active = false;

        if (co->evq)
            ble_npl_eventq_put(co->evq, &co->ev);
        else
            co->ev.fn(&co->ev);
    }
    _nimble_callout_arm();
    sys_hal_irq_restore(state);
}

void ble_npl_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq, ble_npl_event_fn *ev_cb,
                          void *ev_arg)
{
    memset(co, 0, sizeof(*co));
    co->evq = evq;
    ble_npl_event_init(&co->ev, ev_cb, ev_arg);
}

ble_npl_error_t ble_npl_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks)
{
    const uint32_t state = sys_hal_irq_save();

    if (co->active)
        _nimble_callout_unlink(co);

    co->expiry = ble_npl_time_get() + ticks;
    co->active = true;

    struct ble_npl_callout **at = &_nimble_callouts;
    while (*at && ((int32_t)((*at)->expiry - co->expiry) <= 0))
        at = &(*at)->next;
    co->next = *at;
    *at = co;

    _nimble_callout_arm();
    sys_hal_irq_restore(state);
    return BLE_NPL_OK;
}

void ble_npl_callout_stop(struct ble_npl_callout *co)
{
    const uint32_t state = sys_hal_irq_save();
    if (co->active)
    {
        _nimble_callout_unlink(co);
        _nimble_callout_arm();
    }
    sys_hal_irq_restore(state);
}

bool ble_npl_callout_is_active(struct ble_npl_callout *co)
{
    return co->active;
}

ble_npl_time_t ble_npl_callout_get_ticks(struct ble_npl_callout *co)
{
    return co->expiry;
}

ble_npl_time_t ble_npl_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t time)
{
    const int32_t left = (int32_t)(co->expiry - time);
    return (co->active && (left > 0)) ? (ble_npl_time_t)left : 0;
}

void ble_npl_callout_set_arg(struct ble_npl_callout *co, void *arg)
{
    co->ev.arg = arg;
}

// ---- Interrupts NimBLE installs handlers for ----

void ble_npl_hw_set_isr(int irqn, void (*addr)(void))
{
    switch (irqn)
    {
        case RADIO_IRQn:
            _nimble_isr_radio = addr;
            break;

        case RTC0_IRQn:
            _nimble_isr_rtc0 = addr;
            break;

        case RNG_IRQn:
            _nimble_isr_rng = addr;
            break;

        case CCM_AAR_IRQn:
            _nimble_isr_ccm_aar = addr;
            break;

        default:
            assert(0);
            break;
    }
}

void RADIO_IRQHandler(void)
{
    if (_nimble_isr_radio)
        _nimble_isr_radio();
}

void RTC0_IRQHandler(void)
{
    if (_nimble_isr_rtc0)
        _nimble_isr_rtc0();
}

void RNG_IRQHandler(void)
{
    if (_nimble_isr_rng)
        _nimble_isr_rng();
}

void CCM_AAR_IRQHandler(void)
{
    if (_nimble_isr_ccm_aar)
        _nimble_isr_ccm_aar();
}

// ---- HCI, the controller's side to the host's ----

// Interrupts off. False when the ring has no room for the whole packet.
static bool _nimble_rx_put(uint8_t type, const struct os_mbuf *om, const uint8_t *flat, uint16_t len)
{
    const uint16_t used = (uint16_t)(_nimble_rx_head - _nimble_rx_tail);
    if ((uint32_t)used + NIMBLE_PORT_RX_HEADER_LEN + len > NIMBLE_PORT_RX_RING_LEN)
        return false;

    const uint8_t header[NIMBLE_PORT_RX_HEADER_LEN] = {type, (uint8_t)len, (uint8_t)(len >> 8)};
    uint16_t at = _nimble_rx_head;
    for (uint16_t i = 0; i < NIMBLE_PORT_RX_HEADER_LEN; i++)
        _nimble_rx_ring[(at++) % NIMBLE_PORT_RX_RING_LEN] = header[i];

    for (uint16_t i = 0; i < len; i++)
    {
        uint8_t byte;
        if (om)
            os_mbuf_copydata(om, i, 1, &byte);
        else
            byte = flat[i];
        _nimble_rx_ring[(at++) % NIMBLE_PORT_RX_RING_LEN] = byte;
    }
    _nimble_rx_head = at;
    return true;
}

// An HCI event from the controller: [code][length][parameters]
int ble_transport_to_hs_evt_impl(void *buf)
{
    const uint8_t *evt = buf;
    const uint16_t len = (uint16_t)(2u + evt[1]);

    const uint32_t state = sys_hal_irq_save();
    if (!_nimble_rx_put(NRF52_BLE_HCI_EVT, NULL, evt, len))
        _nimble_rx_dropped++;
    sys_hal_irq_restore(state);

    ble_transport_free(buf);
    return 0;
}

// ACL data from the controller, as a chain of its buffers
int ble_transport_to_hs_acl_impl(struct os_mbuf *om)
{
    const uint16_t len = OS_MBUF_PKTLEN(om);

    const uint32_t state = sys_hal_irq_save();
    if (!_nimble_rx_put(NRF52_BLE_HCI_ACL, om, NULL, len))
        _nimble_rx_dropped++;
    sys_hal_irq_restore(state);

    os_mbuf_free_chain(om);
    return 0;
}

int ble_transport_to_hs_iso_impl(struct os_mbuf *om)
{
    os_mbuf_free_chain(om);
    return 0;
}

void ble_transport_hs_init(void)
{
}

bool nrf52_ble_hci_send(uint8_t type, const uint8_t *pkt, uint16_t len)
{
    if (!_nimble_started)
        return false;

    if (type == NRF52_BLE_HCI_CMD)
    {
        if (len > NRF52_BLE_HCI_CMD_MAX_LEN)
            return false;
        uint8_t *buf = ble_transport_alloc_cmd();
        if (!buf)
            return false;
        memcpy(buf, pkt, len);
        return ble_transport_to_ll_cmd(buf) == 0;
    }

    if (type == NRF52_BLE_HCI_ACL)
    {
        struct os_mbuf *om = ble_transport_alloc_acl_from_hs();
        if (!om)
            return false;
        if (os_mbuf_append(om, pkt, len) != 0)
        {
            os_mbuf_free_chain(om);
            return false;
        }
        return ble_transport_to_ll_acl(om) == 0;
    }

    return false;
}

uint16_t nrf52_ble_hci_receive(uint8_t *type, uint8_t *buf, uint16_t max_len)
{
    if (_nimble_rx_head == _nimble_rx_tail)
        return 0;

    uint16_t at = _nimble_rx_tail;
    uint8_t header[NIMBLE_PORT_RX_HEADER_LEN];
    for (uint16_t i = 0; i < NIMBLE_PORT_RX_HEADER_LEN; i++)
        header[i] = _nimble_rx_ring[(at++) % NIMBLE_PORT_RX_RING_LEN];

    const uint16_t len = (uint16_t)(header[1] | (header[2] << 8));
    const uint16_t copy = (len < max_len) ? len : max_len;
    for (uint16_t i = 0; i < len; i++)
    {
        const uint8_t byte = _nimble_rx_ring[(at++) % NIMBLE_PORT_RX_RING_LEN];
        if (i < copy)
            buf[i] = byte;
    }

    *type = header[0];
    _nimble_rx_tail = at;
    return copy;
}

uint32_t nrf52_ble_hci_dropped(void)
{
    return _nimble_rx_dropped;
}

// ---- Start ----

extern void os_msys_init(void);
extern void os_mempool_module_init(void);
extern void ble_ll_init(void);

void nrf52_ble_controller_address(uint8_t out[6])
{
    const uint32_t low = NRF_FICR->DEVICEADDR[0];
    const uint32_t high = NRF_FICR->DEVICEADDR[1];
    out[0] = (uint8_t)low;
    out[1] = (uint8_t)(low >> 8);
    out[2] = (uint8_t)(low >> 16);
    out[3] = (uint8_t)(low >> 24);
    out[4] = (uint8_t)high;
    out[5] = (uint8_t)(high >> 8);
}

bool nrf52_ble_controller_start(void)
{
    if (_nimble_started)
        return true;

    // The link layer's clock is RTC0, on the 32768 Hz clock. Taken from the crystal (which is always
    // running here) it is as steady as the crystal: the chip's own RC oscillator drifts further
    // than a connection's timing allows unless it is calibrated every few seconds.
    // The source only changes while the clock is stopped. The watchdog keeps its own RC oscillator
    // running through this.
    NRF_CLOCK->TASKS_LFCLKSTOP = 1;
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Synth << CLOCK_LFCLKSRC_SRC_Pos;
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    const uint64_t until = sys_hal_now_us() + NIMBLE_PORT_LFCLK_WAIT_US;
    while (!NRF_CLOCK->EVENTS_LFCLKSTARTED && (sys_hal_now_us() < until))
    {
    }

    NVIC_SetPriority(NIMBLE_PORT_TASK_IRQ, NIMBLE_PORT_PRIO_TASK);
    NVIC_ClearPendingIRQ(NIMBLE_PORT_TASK_IRQ);
    NVIC_EnableIRQ(NIMBLE_PORT_TASK_IRQ);

    // As nimble_port_init does, without a host
    os_mempool_module_init();
    os_msys_init();
    ble_ll_init();
    // The link layer waits, inside its task, for the random number generator's interrupt to refill
    // its pool: that interrupt must be able to break in on the task (NimBLE gives it the lowest
    // priority, for a task that is a thread)
    NVIC_SetPriority(RNG_IRQn, NIMBLE_PORT_PRIO_RNG);
    uint8_t address[6];
    nrf52_ble_controller_address(address);
    ble_ll_set_public_addr(address);
    ble_transport_init();
    ble_transport_hs_init();
    hal_timer_init(MYNEWT_VAL(OS_CPUTIME_TIMER_NUM), NULL);
    os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    ble_transport_ll_init();
    // The timer starts radio events: it must not wait behind the link layer's task or USB
    NVIC_SetPriority(RTC0_IRQn, NIMBLE_PORT_PRIO_CPUTIME);

    // What NimBLE's link layer task does before its loop (ble_ll_task). The rest of its state is
    // set by the HCI Reset every host starts with.
    ble_phy_init();
    const int tx_dbm = (MYNEWT_VAL(BLE_LL_TX_PWR_DBM) < MYNEWT_VAL(BLE_LL_TX_PWR_MAX_DBM))
                           ? MYNEWT_VAL(BLE_LL_TX_PWR_DBM)
                           : MYNEWT_VAL(BLE_LL_TX_PWR_MAX_DBM);
    g_ble_ll_tx_power = ble_ll_tx_power_round(tx_dbm);

    _nimble_started = true;
    NVIC_SetPendingIRQ(NIMBLE_PORT_TASK_IRQ);
    return true;
}

#if defined(HOJA_BENCH_LOG)
// Bench: the link layer's counters (nimble/stats/stats.h), as text. Each section is a run of 32-bit
// counters in the order NimBLE declares them (ble_phy.c, ble_ll_conn.c).
#include <stdio.h>

extern uint32_t ble_phy_stats[];
extern uint32_t ble_ll_conn_stats[];

int nrf52_ble_bench_stats(char *out, size_t max)
{
    const uint32_t *phy = ble_phy_stats;
    const uint32_t *conn = ble_ll_conn_stats;
    return snprintf(out, max,
                    "LL rx valid %lu crc %lu no-anchor %lu dup %lu | tx late %lu | data acked %lu failed %lu "
                    "queued %lu l2cap tx %lu ctrl tx %lu empty tx %lu rx data %lu\n",
                    (unsigned long)phy[7], (unsigned long)phy[8], (unsigned long)conn[2], (unsigned long)conn[10],
                    (unsigned long)phy[3], (unsigned long)conn[11], (unsigned long)conn[12],
                    (unsigned long)conn[14], (unsigned long)conn[22], (unsigned long)conn[20],
                    (unsigned long)conn[24], (unsigned long)conn[16]);
}
#endif
