#ifndef FLOSS_CONFIG_HOJA_H
#define FLOSS_CONFIG_HOJA_H

// FLOSS on the nRF52: Bluetooth LE only, one host at a time, over the controller in the same chip
// (src/hal/nrf52/ble)

#define FLOSS_MAX_CONNECTIONS 1

// The controller is handed each packet as it stands: nothing is written in front of it
#define FLOSS_TRANSPORT_MAX_HEADROOM 0

#define FLOSS_ENABLE_LE         1
#define FLOSS_ENABLE_RFCOMM     0
#define FLOSS_ENABLE_SDP_CLIENT 0
#define FLOSS_ENABLE_LINK       0

#define FLOSS_MAX_BONDS 8

// What a Switch 2 asks for, and its own controllers grant
#define FLOSS_ATT_MTU 512

// On the bench: FLOSS's log and every HCI packet, read out over the config interface
#if defined(HOJA_BENCH_LOG)
#define FLOSS_LOG_LEVEL     FLOSS_LOG_INFO
#define FLOSS_ENABLE_SNOOP  1
#else
#define FLOSS_LOG_LEVEL     FLOSS_LOG_WARN
#define FLOSS_ENABLE_SNOOP  0
#endif
#define FLOSS_TRACE_ENTRIES 0
#define FLOSS_ENABLE_STATS  0

#endif
