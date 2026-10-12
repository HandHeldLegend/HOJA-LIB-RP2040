#ifndef FLOSS_CONFIG_HOJA_H
#define FLOSS_CONFIG_HOJA_H

// FLOSS configuration for HOJA (FLOSS_USER_CONFIG_FILE). Only what differs from floss_config.h.

// One host link, plus room for a host calling while another link closes
#define FLOSS_MAX_CONNECTIONS 2

// The CYW43 shared-bus transport writes a 4-byte header in front of each packet
#define FLOSS_TRANSPORT_MAX_HEADROOM 4

// Classic HID only
#define FLOSS_ENABLE_LE     0
#define FLOSS_ENABLE_RFCOMM 0
// Link test builds use FLOSS's pad <-> dongle link profile
#if defined(HOJA_BT_FLOSS_LINK_TEST) && HOJA_BT_FLOSS_LINK_TEST
#define FLOSS_ENABLE_LINK   1
#else
#define FLOSS_ENABLE_LINK   0
#endif

// A Switch, a Wii and a few PCs
#define FLOSS_MAX_BONDS 8

// HOJA retries connections itself, per mode
#define FLOSS_HID_RECONNECT_RETRIES 0

#define FLOSS_LOG_LEVEL     FLOSS_LOG_WARN
#define FLOSS_TRACE_ENTRIES 0
#define FLOSS_ENABLE_STATS  0
// Diagnostic test builds watch HCI events through the snoop hook
#if defined(HOJA_BT_FLOSS_DIAG) && HOJA_BT_FLOSS_DIAG
#define FLOSS_ENABLE_SNOOP  1
#else
#define FLOSS_ENABLE_SNOOP  0
#endif

#endif
