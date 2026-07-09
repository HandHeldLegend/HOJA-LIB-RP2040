#ifndef TRANSPORT_BT_H
#define TRANSPORT_BT_H

#include <stdbool.h>
#include <stdint.h>

#include "cores/cores.h"

/** Part-status encoding shared with bluetoothInfoStatic_s.wireless_part_status. */
#define TRANSPORT_WIRELESS_PART_NA     0u
#define TRANSPORT_WIRELESS_PART_ERROR  1u
#define TRANSPORT_WIRELESS_PART_OK     2u

typedef struct
{
    uint8_t bdr_supported;
    uint8_t ble_supported;
    uint8_t external_update_supported;
} transport_bt_static_caps_s;

void transport_bt_stop();
bool transport_bt_init(core_params_s *params);
void transport_bt_task(uint64_t timestamp);

void transport_bt_static_get_caps(transport_bt_static_caps_s *caps);
uint8_t transport_bt_static_part_status(void);
uint16_t transport_bt_static_external_version(void);

#endif
