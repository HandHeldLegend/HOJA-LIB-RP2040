#ifndef HOJA_NIMBLE_NPL_OS_H
#define HOJA_NIMBLE_NPL_OS_H

// NimBLE's porting layer with no operating system under it (src/hal/nrf52/ble/nimble_port.c).
//
// NimBLE's link layer is written as a task that takes events from a queue. Here the queue is drained
// from a low-priority interrupt instead, so the link layer keeps up with the radio whatever the
// main loop is doing. There is only that one context besides interrupts, so a mutex has nothing to
// wait for.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nimble/nimble_npl_atomic_builtin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_NPL_OS_ALIGNMENT 4
#define BLE_NPL_TIME_FOREVER UINT32_MAX

// A tick is a millisecond
typedef uint32_t ble_npl_time_t;
typedef int32_t  ble_npl_stime_t;

struct ble_npl_event
{
    ble_npl_event_fn     *fn;
    void                 *arg;
    struct ble_npl_event *next;
    bool                  queued;
};

struct ble_npl_eventq
{
    struct ble_npl_event *head;
    struct ble_npl_event *tail;
};

struct ble_npl_callout
{
    struct ble_npl_event    ev;
    struct ble_npl_eventq  *evq;
    struct ble_npl_callout *next;
    ble_npl_time_t          expiry;
    bool                    active;
};

struct ble_npl_mutex
{
    uint8_t unused;
};

struct ble_npl_sem
{
    volatile uint16_t count;
};

#ifdef __cplusplus
}
#endif

#endif
