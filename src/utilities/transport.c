#include "utilities/transport.h"
#include "utilities/callback.h"

SNAPSHOT_TYPE(transport, transport_status_s);
snapshot_transport_t _transport_snap;

transport_profile_s _transport_profile = {0};

time_callback_t _transport_task = NULL;

transport_status_s transport_get_status(void)
{
    transport_status_s tmp;
    snapshot_transport_read(&_transport_snap, &tmp);
    return tmp;
}

transport_profile_s* transport_active_profile()
{
    return &_transport_profile;
}

void transport_init(transport_profile_s profile)
{
    _transport_profile = profile;

    
}

