#include "utilities/transport.h"
#include "utilities/callback.h"
#include "utilities/crosscore_snapshot.h"

SNAPSHOT_TYPE(transport, transport_status_s);
snapshot_transport_t _transport_snap;

void transport_get_status(transport_status_s* out)
{
    snapshot_transport_read(&_transport_snap, out);
}

void transport_set_connected(bool connected)
{
    transport_status_s tmp;
    snapshot_transport_read(&_transport_snap, &tmp);
    tmp.connected = connected;
    snapshot_transport_write(&_transport_snap, &tmp);
}

void transport_set_player_number(uint8_t player_number)
{
    transport_status_s tmp;
    snapshot_transport_read(&_transport_snap, &tmp);
    tmp.player_number = player_number;
    snapshot_transport_write(&_transport_snap, &tmp);
}

void transport_set_polling_rate_us(uint32_t polling_rate_us);

