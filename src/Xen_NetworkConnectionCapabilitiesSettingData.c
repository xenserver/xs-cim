// Copyright (C) 2008-2009 CitrixSystems Inc
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <cmpiutil.h>
#include <cmpift.h>
#include <RASDs.h>
#include "providerinterface.h"

typedef struct {
    unsigned int currentsettingdatanum;
    unsigned int currentnetnum;
    xen_network_set *network_set;
} local_net_cap_list;

typedef struct {
    unsigned long long reservation;
    unsigned long long limit;
    unsigned long long quantity;
    unsigned long weight;
    char instanceid[MAX_INSTANCEID_LEN];
    char poolid[MAX_INSTANCEID_LEN];
} local_net_cap_resource;

//static const char * classname = "Xen_NetworkConnectionCapabilitiesSettingData";
static const char *keys[] = {"InstanceID"}; 
static const char *key_property = "InstanceID";

/*********************************************************
 ************ Internal functions ****************************
 ********************************************************/
static CMPIrc 
populate_resource(
    xen_utils_session *session,
    provider_resource *resource,
    xen_network res_pool,  
    int settingdatanum
)
{
    CMPIrc rc = CMPI_RC_ERR_FAILED;
    char *instanceid;
    char *net_uuid = NULL;

    if(resource == NULL) return CMPI_RC_ERR_FAILED;
    if(!xen_network_get_uuid(session->xen, &net_uuid, res_pool))
        goto Exit;

    local_net_cap_resource *ctx = calloc(1, sizeof(local_net_cap_resource));
    if(ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Network = %s",net_uuid)); 
    rc = CMPI_RC_OK;
    switch(settingdatanum)
    {
    /* You can only request 1 network connection at a time */
    case 0: /* Minimum */
        instanceid = "NetworkConnectionMinimum";
        ctx->reservation = 1;
        ctx->limit = 1;
        ctx->quantity = 1;
        //ctx->weight = Not applicable
        break;

    case 1: /* Maximum */
        instanceid = "NetworkConnectionMaximum";
        ctx->reservation = 1;
        ctx->limit = 1;
        ctx->quantity = 1;
        //ctx->weight = 1;
        break;

    case 2: /* Increment */
        instanceid = "NetworkConnectionIncrement";
        ctx->reservation = 1;
        ctx->limit = 1;
        ctx->quantity = 1;
        //ctx->weight = not applicable
        break;

    case 3: /* Default */
        instanceid = "NetworkConnectionDefault";
        ctx->reservation = 1;
        ctx->limit = 1;
        ctx->quantity = 1;
        //ctx->weight = not applicable
        break;
    default:
        rc = CMPI_RC_ERR_FAILED;
        free(ctx);
        goto Exit;
    }

    _CMPICreateNewDeviceInstanceID(ctx->instanceid, MAX_INSTANCEID_LEN-1, net_uuid, instanceid);
    _CMPICreateNewSystemInstanceID(ctx->poolid, MAX_INSTANCEID_LEN-1, net_uuid );
    resource->ctx = ctx;
Exit:
    if(!session->xen->ok)
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    if(net_uuid)
        free(net_uuid);
    return rc;
}
/*********************************************************
 ************ Provider functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    return key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    return keys;
}
/********************************************************
 * Function to enumerate provider specific resource
 *
 * @param session - handle to a xen_utils_session object
 * @param resources - pointer to the provider_resource_list
 *   object, the provider specific resource defined above
 *   is a member of this struct
 * @return CMPIrc error codes
 ********************************************************/
static CMPIrc xen_resource_list_enum(
    xen_utils_session *session, 
    provider_resource_list *resources
    )
{
    xen_network_set *net_set = NULL;
    if(!xen_network_get_all(session->xen, &net_set) || net_set->size ==0)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Unable to obtain network list"));
        return CMPI_RC_ERR_FAILED;
    }
    local_net_cap_list * ctx = calloc(sizeof(local_net_cap_list), 1);
    if(ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->network_set = net_set;
    ctx->currentsettingdatanum = 0;
    ctx->currentnetnum = 0;
    resources->ctx = ctx;
    return CMPI_RC_OK;
}
/*******************************************************************
 * Function to cleanup provider specific resource, this function is
 * called at various places in Xen_ProviderGeneric.c
 *
 * @param resources - handle to the provider_resource_list to be
 *    be cleaned up. Clean up the provider specific part of the
 *    resource.
 * @return CMPIrc error codes
 *******************************************************************/
static CMPIrc xen_resource_list_cleanup(
    provider_resource_list *resources
    )
{
    if(resources && resources->ctx) {
        local_net_cap_list *ctx = (local_net_cap_list *)resources->ctx;
        xen_network_set_free(ctx->network_set);
        free(ctx);
    }
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Function to get the next provider specific resource in the resource list
 *
 * @param resources_list - handle to the provide_resource_list object
 * @param session - handle to the xen_utils_session object
 * @param prov_res - handle to the next provider_resource to be filled in.
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_record_getnext(
    provider_resource_list *resources_list,/* in */
    xen_utils_session *session,/* in */
    provider_resource *prov_res /* in , out */
    )
{
    CMPIrc rc = CMPI_RC_ERR_NOT_FOUND;
    local_net_cap_list *ctx = (local_net_cap_list *)resources_list->ctx;
    if(ctx->currentnetnum >= ctx->network_set->size)
        return rc;

    rc = populate_resource(
             resources_list->session,
             prov_res,
             ctx->network_set->contents[ctx->currentnetnum],
             ctx->currentsettingdatanum);

    /* Move to the next setting data for next time. */
    if(ctx->currentsettingdatanum < 3) {
        ctx->currentsettingdatanum++;
    }
    else {
        ctx->currentsettingdatanum = 0;
        ctx->currentnetnum++; /* start with the next net*/
    }
    return rc;
}
/*****************************************************************************
 * Function to cleanup the resource
 *
 * @param - provider_resource to be freed
 * @return CMPIrc error codes
****************************************************************************/
static CMPIrc xen_resource_record_cleanup(
    provider_resource *prov_res
    )
{  
    if(prov_res->ctx)
        free(prov_res->ctx);
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Function to get a provider specific resource identified by an id
 *
 * @param res_uuid - resource identifier for the provider specific resource
 * @param session - handle to the xen_utils_session object
 * @param prov_res - provide_resource object to be filled in with the provider
 *                   specific resource
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_record_get_from_id(
    char *res_uuid, /* in */
    xen_utils_session *session, /* in */
    provider_resource *prov_res /* in , out */
    )
{
    char buf[MAX_INSTANCEID_LEN];
    _CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf));
    xen_network network;

    if(!xen_network_get_by_uuid(session->xen, &network, res_uuid)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));

    /* Generate raw data for the single resource. */
    int settingdatanum;
    if(strcmp(buf, "NetworkConnectionMinimum") == 0)
        settingdatanum = 0;
    else if(strcmp(buf, "NetworkConnectionMaximum") == 0)
        settingdatanum = 1;
    else if(strcmp(buf, "NetworkConnectionIncrement") == 0)
        settingdatanum = 2;
    else if(strcmp(buf, "NetworkConnectionDefault") == 0)
        settingdatanum = 3;
    else
        return CMPI_RC_ERR_FAILED;

    populate_resource(session, prov_res, network, settingdatanum);
    xen_network_free(network);
    return CMPI_RC_OK;
}
/************************************************************************
 * Function that sets the properties of a CIM object with values from the
 * provider specific resource.
 *
 * @param resource - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @return CMPIrc return values
*************************************************************************/
static CMPIrc xen_resource_set_properties(
    provider_resource *prov_res, 
    CMPIInstance *inst
    )
{
    local_net_cap_resource *resource = (local_net_cap_resource *)prov_res->ctx;
    /* Populate the instance's properties with the backend data */
    //CMSetProperty(inst, "Address",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"count", CMPI_chars);
    //CMSetProperty(inst, "AutomaticAllocation",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "AutomaticDeallocation",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Network Connection Allocation capabilities", CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Connection",(CMPIValue *)&arr, CMPI_charsA);
    int consumerVisibilty = DMTF_ConsumerVisibility_Virtualized; 
    CMSetProperty(inst, "ConsumerVisibility",(CMPIValue *)&consumerVisibilty, CMPI_uint16);
    //CMSetProperty(inst, "Description",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ElementName",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(instance, "HostResource",(CMPIValue *)&arr, CMPI_charsA);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)resource->instanceid, CMPI_chars);
    CMSetProperty(inst, "Limit",(CMPIValue *)&(resource->limit), CMPI_uint64);
    int mappingBehavior = DMTF_MappingBehavior_Dedicated;
    CMSetProperty(inst, "MappingBehavior",(CMPIValue *)&mappingBehavior, CMPI_uint16);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Parent",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "PoolID",(CMPIValue *)resource->poolid, CMPI_chars);
    CMSetProperty(inst, "Reservation",(CMPIValue *)&(resource->reservation), CMPI_uint64);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    int type = DMTF_ResourceType_Ethernet_Connection;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&type, CMPI_uint16);
    CMSetProperty(inst, "VirtualQuantity",(CMPIValue *)&(resource->quantity), CMPI_uint64);
    CMSetProperty(inst, "Weight",(CMPIValue *)&(resource->weight), CMPI_uint32);

    return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_NetworkConnectionCapabilitiesSettingData)

