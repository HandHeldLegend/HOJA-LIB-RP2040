#ifndef HOJA_NIMBLE_SYSCFG_H
#define HOJA_NIMBLE_SYSCFG_H

// NimBLE's settings for its controller on the nRF52, as HOJA builds it: a link layer with no host
// of its own (the host is FLOSS, over HCI) and no operating system under it.
//
// What is set here wins. Everything else takes the default from NimBLE's own syscfg.yml files
// (nimble_syscfg_defaults.h, generated at configure time by cmake/nimble_syscfg.py), then from the
// header NimBLE's porting folder ships.

// A peripheral that advertises; it never scans or starts connections
#define MYNEWT_VAL_BLE_ROLE_PERIPHERAL  (1)
#define MYNEWT_VAL_BLE_ROLE_BROADCASTER (1)
#define MYNEWT_VAL_BLE_ROLE_CENTRAL     (0)
#define MYNEWT_VAL_BLE_ROLE_OBSERVER    (0)
#define MYNEWT_VAL_BLE_MAX_CONNECTIONS  (1)

#define MYNEWT_VAL_BLE_CONTROLLER       (1)
#define MYNEWT_VAL_BLE_HOST             (0)
#define MYNEWT_VAL_BLE_VERSION          (52)

// The 2M PHY and channel selection #2, which a Switch 2 moves its controllers to, and whole-size
// packets from the start
#define MYNEWT_VAL_BLE_PHY_2M                       (1)
#define MYNEWT_VAL_BLE_LL_CFG_FEAT_LE_CSA2          (1)
#define MYNEWT_VAL_BLE_LL_CONN_INIT_MAX_TX_BYTES    (251)

// No vendor HCI commands. A failed check in the link layer is an ordinary assert: NimBLE's other way
// (a vendor event, then a loop forever inside the interrupt) would never reach the host here.
#define MYNEWT_VAL_BLE_HCI_VS                       (0)
#define MYNEWT_VAL_BLE_LL_HCI_VS_EVENT_ON_ASSERT    (0)

// Timing: the link layer's clock is RTC0 at 32768 Hz (NimBLE's timer 5), with TIMER0 beside it for
// the radio. The crystal is always running, so the radio needs no warm-up.
#define MYNEWT_VAL_OS_CPUTIME_FREQ      (32768)
#define MYNEWT_VAL_OS_CPUTIME_TIMER_NUM (5)
#define MYNEWT_VAL_TIMER_0              (0)
#define MYNEWT_VAL_TIMER_1              (0)
#define MYNEWT_VAL_TIMER_2              (0)
#define MYNEWT_VAL_TIMER_3              (0)
#define MYNEWT_VAL_TIMER_4              (0)
#define MYNEWT_VAL_TIMER_5              (1)
#define MYNEWT_VAL_BLE_LL_RFMGMT_ENABLE_TIME (0)

// HCI stays inside the firmware: the controller's side is NimBLE's own, the host's side is ours
// (src/hal/nrf52/ble)
#define MYNEWT_VAL_BLE_TRANSPORT                (1)
#define MYNEWT_VAL_BLE_TRANSPORT_LL             (1)
#define MYNEWT_VAL_BLE_TRANSPORT_LL__native     (1)
#define MYNEWT_VAL_BLE_TRANSPORT_LL__socket     (0)
#define MYNEWT_VAL_BLE_TRANSPORT_HS             (1)
#define MYNEWT_VAL_BLE_TRANSPORT_HS__custom     (1)
#define MYNEWT_VAL_BLE_TRANSPORT_HS__native     (0)

#define MYNEWT_VAL_MCU_NATIVE           (0)

#include "nimble_syscfg_defaults.h"
#include_next "syscfg/syscfg.h"

#endif
