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

// The radio is running this boot (a wireless mode, or BLE config in a wired one): probing it would
// drop its links. False where no Bluetooth HAL can start it in a wired mode.
bool transport_bt_radio_up(void);

// Auto: start the radio while a wired check runs (the N64 wait on battery), so a wireless mode
// after it finds the radio up; off again when the check finds a wired console. Each runs the
// blocking start or stop once.
void transport_bt_radio_warm(void);
void transport_bt_radio_off(void);

// A host is saved for this mode (Switch, Wii or SInput): Auto calls it
bool transport_bt_host_paired(core_reportformat_t format);

// Forget the host saved for this mode (Switch, Wii or SInput): its address, its keys, saved at once.
// False when this Bluetooth backend can't.
bool transport_bt_forget_supported(void);
bool transport_bt_forget_host(core_reportformat_t format);

#if HOJA_BT_USES_FLOSS
// Wired modes: BLE config on Plus + Minus, the radio up for LE alone (bluetooth_hal_floss.c)
void transport_bt_config_task(uint64_t timestamp);
#endif

void transport_bt_static_get_caps(transport_bt_static_caps_s *caps);
uint8_t transport_bt_static_part_status(void);
uint16_t transport_bt_static_external_version(void);

#endif
