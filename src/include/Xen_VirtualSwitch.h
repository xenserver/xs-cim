#include "cmpidt.h"

#include "xen_utils.h"


extern int vssd_to_network_rec(
    const CMPIBroker* broker,
    CMPIInstance *vssd,
    xen_network_record** net_rec_out,
    CMPIStatus *status
    );

extern CMPIObjectPath* virtual_switch_create_ref(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_network network,
    CMPIStatus *status
    );
