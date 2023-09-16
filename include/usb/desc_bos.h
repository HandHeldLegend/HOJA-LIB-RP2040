#ifndef DESC_BOS_H
#define DESC_BOS_H

#include "hoja_includes.h"

enum
{
  VENDOR_REQUEST_WEBUSB = 1,
  VENDOR_REQUEST_MICROSOFT = 2
};

#define ITF_NUM_VENDOR 1
#define GC_ITF_NUM_VENDOR 0

extern uint8_t const desc_bos[];
extern uint8_t const desc_ms_os_20[];
extern uint8_t const gc_desc_ms_os_20[];

#endif