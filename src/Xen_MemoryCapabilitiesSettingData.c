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

#include <cmpift.h>
#include "cmpiutil.h"
#include "RASDs.h"
#include "providerinterface.h"

typedef struct {
    unsigned int currentsettingdatanum;
    unsigned int currenthostnum;
    xen_host_set *host_set;
} local_mem_cap_list;

typedef struct {
    unsigned long long reservation;
    unsigned long long limit;
    unsigned long long quantity;
    unsigned long weight;
    char instanceid[MAX_INSTANCEID_LEN];
    char poolid[MAX_INSTANCEID_LEN];
} local_mem_cap_resource;

//static const char * classname = "Xen_MemoryCapabilitiesSettingData";
static const char *keys[] = {"InstanceID"}; 
static const char *key_property = "InstanceID";
/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname)
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
/*********************************************************
 ************ Extra functions ****************************
 ********************************************************/
static CMPIrc populate_resource(
    xen_utils_session *session,
    provider_resource *resource,
    xen_host res_pool,
    int settingdatanum
    )
{
    CMPIrc rc = CMPI_RC_ERR_FAILED;
    uint64_t mem_free = 0, mem_total = 0;
    char *instanceid;
    xen_host_record *host_rec = NULL;
    xen_host_metrics host_metrics = NULL;

    local_mem_cap_resource *ctx = (local_mem_cap_resource *)calloc(1, sizeof(local_mem_cap_resource));
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    xen_host_get_record(session->xen, &host_rec, res_pool);
    if (!xen_host_get_metrics(session->xen, &host_metrics, res_pool))
        goto Exit;
    xen_host_metrics_record *host_metrics_rec = NULL;
    if (!xen_host_metrics_get_record(session->xen, &host_metrics_rec, host_metrics))
        goto Exit;
    else {
        mem_free = host_metrics_rec->memory_free;
        mem_total = host_metrics_rec->memory_total;
    }

    rc = CMPI_RC_OK;
    switch (settingdatanum) {
    case 0: /* Minimum */
        instanceid = "MemoryMinimum";
        ctx->reservation = 64 << 20; /* 16MB */
        ctx->limit = 64 << 20;
        ctx->quantity = 64 << 20;
        //(resource)->weight = 1;
        break;
    case 1: /* Maximum */
        instanceid = "MemoryMaximum";
        ctx->reservation = mem_free; /* 1GB */
        ctx->limit = mem_total;
        ctx->quantity = mem_free;
        //(resource)->weight = 1;
        break;
    case 2: /* Increment */
        instanceid = "MemoryIncrement";
        ctx->reservation = 64 << 20;
        ctx->limit = 64 << 20;
        ctx->quantity = 64 << 20;
        //(resource)->weight = 1;
        break;
    case 3: /* Default */
        instanceid = "MemoryDefault";
        ctx->reservation = 256 << 20; /* 256 MB */
        ctx->limit = 256 << 20;
        ctx->quantity = 256 << 20;
        //(resource)->weight = 1;
        break;
    default:
        rc = CMPI_RC_ERR_FAILED;
        goto Exit;
    }
    _CMPICreateNewDeviceInstanceID(ctx->instanceid, MAX_INSTANCEID_LEN-1, host_rec->uuid, instanceid);
    _CMPICreateNewDeviceInstanceID(ctx->poolid, MAX_INSTANCEID_LEN-1, host_rec->uuid, "MemoryPool");
    resource->ctx = ctx;
    Exit:
    if (!session->xen->ok)
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    if (host_metrics_rec)
        xen_host_metrics_record_free(host_metrics_rec);
    if (host_metrics)
        xen_host_metrics_free(host_metrics);
    if (host_rec)
        xen_host_record_free(host_rec);
    if (resource->ctx != ctx) {
        // Context not used, free again
        free(ctx);
    }

    return rc;
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
    xen_host_set *host_set = NULL;
    if (!xen_host_get_all(session->xen, &host_set) || host_set->size ==0) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Unable to obtain host list"));
        return CMPI_RC_ERR_FAILED;
    }
    local_mem_cap_list *ctx = (local_mem_cap_list *)calloc(sizeof(local_mem_cap_list), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    ctx->host_set = host_set;
    ctx->currentsettingdatanum = 0;
    ctx->currenthostnum = 0;
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
static CMPIrc xen_resource_list_cleanup(provider_resource_list *resources)
{
    if (resources && resources->ctx) {
        local_mem_cap_list *ctx = (local_mem_cap_list *)resources->ctx;
        if (ctx->host_set)
            xen_host_set_free(ctx->host_set);
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
    local_mem_cap_list *ctx = (local_mem_cap_list *)resources_list->ctx;

    if (ctx->currenthostnum >= ctx->host_set->size)
        return rc;

    rc = populate_resource(resources_list->session,
             prov_res,
             ctx->host_set->contents[ctx->currenthostnum],
             ctx->currentsettingdatanum);

    /* Move to the next setting data for next time. */
    if (ctx->currentsettingdatanum < 3) {
        ctx->currentsettingdatanum++;
    }
    else {
        ctx->currentsettingdatanum = 0;
        ctx->currenthostnum++; /* start with the next host*/
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
    if (prov_res->ctx)
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
    xen_host host = NULL;
    if (!xen_host_get_by_uuid(session->xen, &host, buf)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));

    /* Generate raw data for the single resource. */
    int settingdatanum;
    if (strcmp(buf, "MemoryMinimum") == 0)
        settingdatanum = 0;
    else if (strcmp(buf, "MemoryMaximum") == 0)
        settingdatanum = 1;
    else if (strcmp(buf, "MemoryIncrement") == 0)
        settingdatanum = 2;
    else if (strcmp(buf, "MemoryDefault") == 0)
        settingdatanum = 3;
    else {
        return CMPI_RC_ERR_FAILED;
    }
    populate_resource(session, prov_res, host, settingdatanum);
    xen_host_free(host);
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
    local_mem_cap_resource *resource = (local_mem_cap_resource *)prov_res->ctx;
    /* Populate the instance's properties with the backend data */
    //CMSetProperty(inst, "Address",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"Bytes", CMPI_chars);
    //CMSetProperty(inst, "AutomaticAllocation",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "AutomaticDeallocation",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Memory Allocation capabilities", CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Connection",(CMPIValue *)&arr, CMPI_charsA);
    int consumerVisibilty = DMTF_ConsumerVisibility_Passed_Through;
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
    int type = DMTF_ResourceType_Memory;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&type, CMPI_uint16);
    CMSetProperty(inst, "VirtualQuantity",(CMPIValue *)&(resource->quantity), CMPI_uint64);
    CMSetProperty(inst, "Weight",(CMPIValue *)&(resource->weight), CMPI_uint32);

    return CMPI_RC_OK;
}
/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_MemoryCapabilitiesSettingData)

    
