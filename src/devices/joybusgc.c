#include <stdint.h>
#include <stdbool.h>
#include "cores/cores.h"

typedef void(*jbgc_rumble_t)(bool rumble_en);
typedef void(*jbgc_connected_t)(bool connected);

jbgc_rumble_t _transport_joybusgc_rumble(bool rumble_en)
{
    uint8_t rumble_value = rumble_en ? 235 : 0;
    uint8_t evt_data[3] = {CORE_EVT_ERMRUMBLE, rumble_value, rumble_value};
    cores_evt_cb(evt_data, 3);
}

jbgc_connected_t _transport_joybusgc_connected(bool connected)
{
    uint8_t connstat = connected ? CORE_CONNECTION_CONNECTED : CORE_CONNECTION_DISCONNECTED;
    uint8_t evt_data[2] = {CORE_EVT_CONNECTIONCHANGE, connstat};
    cores_evt_cb(evt_data, 2);
}

typedef struct
{
    jbgc_rumble_t       rumble_cb;
    jbgc_connected_t    connected_cb;
} transport_joybusgc_params_s;

bool joybusgc_transport_task(uint64_t timestamp)
{

}

bool joybusgc_transport_init()
{
    const transport_joybusgc_params_s params = {
        .connected_cb = _transport_joybusgc_connected,
        .rumble_cb = _transport_joybusgc_rumble,
    };
}