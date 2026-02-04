#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "cores/cores.h"

typedef void(*jb64_rumble_t)(bool rumble_en);
typedef void(*jb64_connected_t)(bool connected);

typedef struct
{
    jb64_rumble_t       rumble_cb;
    jb64_connected_t    connected_cb;
} transport_joybus64_params_s;

jb64_rumble_t _transport_joybus64_rumble(bool rumble_en)
{
    uint8_t rumble_value = rumble_en ? 235 : 0;
    uint8_t evt_data[3] = {CORE_EVT_ERMRUMBLE, rumble_value, rumble_value};
    cores_evt_cb(evt_data, 3);
}

jb64_connected_t _transport_joybus64_connected(bool connected)
{
    uint8_t connstat = connected ? CORE_CONNECTION_CONNECTED : CORE_CONNECTION_DISCONNECTED;
    uint8_t evt_data[2] = {CORE_EVT_CONNECTIONCHANGE, connstat};
    cores_evt_cb(evt_data, 2);
}

void transport_joybus64_task(uint64_t timestamp)
{
    
}

void transport_joybus64_init()
{

}