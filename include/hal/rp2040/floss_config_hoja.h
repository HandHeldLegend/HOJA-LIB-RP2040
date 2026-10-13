#ifndef FLOSS_CONFIG_HOJA_H
#define FLOSS_CONFIG_HOJA_H

// FLOSS configuration for HOJA (FLOSS_USER_CONFIG_FILE). Only what differs from floss_config.h.

// One host link, plus room for a host calling while another link closes
#define FLOSS_MAX_CONNECTIONS 2

// The CYW43 shared-bus transport writes a 4-byte header in front of each packet
#define FLOSS_TRANSPORT_MAX_HEADROOM 4

// Classic HID, and the link profile to a HOJA dongle or the PC app
// LE only in the bench build that measures advertising next to Classic (HOJA_BT_LE_AB)
#if defined(HOJA_BT_LE_AB)
#define FLOSS_ENABLE_LE     1
#else
#define FLOSS_ENABLE_LE     0
#endif
#define FLOSS_ENABLE_RFCOMM 0
#define FLOSS_ENABLE_SDP_CLIENT 0

// Link mode's live report: sequence, sample time, length and a whole 64-byte report (dongle_link.h)
#define FLOSS_HID_MAX_REPORT 72
#define FLOSS_ENABLE_LINK   1

// A live report to the dongle is a length byte plus up to 64 bytes (dongle_link.h)
#define FLOSS_LINK_MAX_REPORT 65

// Looking for a dongle: one 2.56 s inquiry round at a time, so Auto mode moves on quickly
#define FLOSS_LINK_INQUIRY_LENGTH 2u

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
