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
#include <cmpiutil.h>
#include <RASDs.h>
#include "providerinterface.h"

typedef struct {
    unsigned int currentsettingdatanum;
    unsigned int currentsrnum;
    xen_sr_set *sr_set;
} local_sr_cap_list;

typedef struct {
    unsigned long long reservation;
    unsigned long long limit;
    unsigned long long quantity;
    unsigned long weight;
    char instanceid[MAX_INSTANCEID_LEN];
    char poolid[MAX_INSTANCEID_LEN];
} local_sr_cap_resource;

//static const char * classname = "Xen_StorageCapabilitiesSettingData";
static const char *keys[] = {"InstanceID"}; 
static const char *key_property = "InstanceID";

/*********************************************************
 ************ Internal functions ****************************
 ********************************************************/
static CMPIrc 
populate_resource(
    xen_utils_session *session,
    provider_resource *resource,
    xen_sr res_pool,  
    int settingdatanum
)
{
    _SBLIM_ENTER(" populate_resource");
    CMPIrc rc = CMPI_RC_ERR_FAILED;
    char *instanceid;
    xen_sr_record *sr_rec = NULL;

    if(resource == NULL) return CMPI_RC_ERR_FAILED;
    if(!xen_sr_get_record(session->xen, &sr_rec, res_pool))
        goto Exit;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("SR = %s",sr_rec->uuid)); 
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("physical size = %ld",sr_rec->physical_size)); 
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("physical util = %ld",sr_rec->physical_utilisation));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("virtual allocation = %ld",sr_rec->virtual_allocation));

    local_sr_cap_resource* ctx = calloc(1, sizeof(local_sr_cap_resource));
    if(ctx == NULL)
        goto Exit;
    rc = CMPI_RC_OK;
    switch(settingdatanum)
    {
    case 0: /* Minimum */
        instanceid = "StorageMinimum";
        ctx->reservation = (1 << 29); /* 256 MB */
        ctx->limit = (1 << 29);
        ctx->quantity = (1 << 29);
        //ctx->weight = Not applicable
        break;

    case 1: /* Maximum */
        instanceid = "StorageMaximum";
        ctx->reservation = (unsigned long long) sr_rec->physical_size;
        ctx->limit = (unsigned long long) sr_rec->physical_size;
        ctx->quantity = (unsigned long long) sr_rec->physical_size;
        //ctx->weight = 1;
        break;

    case 2: /* Increment */
        instanceid = "StorageIncrement";
        ctx->reservation = (1 << 29); /* 512 MB */
        ctx->limit = (1 << 29);
        ctx->quantity = (1 << 29);
        //ctx->weight = not applicable
        break;

    case 3: /* Default */
        instanceid = "StorageDefault";
        ctx->reservation = (unsigned long long) sr_rec->physical_size;
        ctx->limit = (unsigned long long) sr_rec->physical_size;
        ctx->quantity = (unsigned long long) sr_rec->physical_size;
        //ctx->weight = not applicable
        break;
    default:
        rc = CMPI_RC_ERR_FAILED;
        free(ctx);
        goto Exit;
    }

    _CMPICreateNewDeviceInstanceID(ctx->instanceid, 
                                   MAX_INSTANCEID_LEN-1, sr_rec->uuid, instanceid);
    _CMPICreateNewSystemInstanceID(ctx->poolid, 
                                   MAX_INSTANCEID_LEN-1, sr_rec->uuid );
    resource->ctx = ctx;
Exit:
    if(!session->xen->ok)
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    if(sr_rec)
        xen_sr_record_free(sr_rec);

    _SBLIM_RETURN(rc);
}
/******************************************************************************
 ************ Provider Export functions ***************************************
 *****************************************************************************/
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
/******************************************************************************
 * Function to enumerate provider specific resource
 *
 * @param session - handle to a xen_utils_session object
 * @param resources - pointer to the provider_resource_list
 *   object, the provider specific resource defined above
 *   is a member of this struct
 * @return CMPIrc error codes
 *****************************************************************************/
static CMPIrc xen_resource_list_enum(
    xen_utils_session *session, 
    provider_resource_list *resources
    )
{
    xen_sr_set *sr_set = NULL;
    if(!xen_sr_get_all(session->xen, &sr_set) || sr_set->size ==0)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Unable to obtain sr list"));
        return CMPI_RC_ERR_FAILED;
    }
    local_sr_cap_list* ctx = calloc(1, sizeof(local_sr_cap_list));
    if(ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    ctx->sr_set = sr_set;
    ctx->currentsettingdatanum = 0;
    ctx->currentsrnum = 0;
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
        xen_sr_set_free(((local_sr_cap_list *)resources->ctx)->sr_set);
        free(resources->ctx);
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
    xen_utils_session *session,            /* in */
    provider_resource *prov_res            /* in , out */
    )
{
    CMPIrc rc = CMPI_RC_ERR_NOT_FOUND;
    local_sr_cap_list *ctx = (local_sr_cap_list *)resources_list->ctx;
    if(ctx->currentsrnum >= ctx->sr_set->size)
        return rc;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("getnext: %d (%d of %d)", ctx->currentsettingdatanum, ctx->currentsrnum, ctx->sr_set->size));
    rc = populate_resource(
            resources_list->session,
            prov_res,
            ctx->sr_set->contents[ctx->currentsrnum],
            ctx->currentsettingdatanum);

    /* Move to the next setting data for next time. */
    if(ctx->currentsettingdatanum < 3) {
        ctx->currentsettingdatanum++;
    }
    else {
        ctx->currentsettingdatanum = 0;
        ctx->currentsrnum++; /* start with the next sr */
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
    xen_sr sr;
    if(!xen_sr_get_by_uuid(session->xen, &sr, res_uuid)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }

    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));

    /* Generate raw data for the single resource. */
    int settingdatanum;
    if(strcmp(buf, "StorageMinimum") == 0)
        settingdatanum = 0;
    else if(strcmp(buf, "StorageMaximum") == 0)
        settingdatanum = 1;
    else if(strcmp(buf, "StorageIncrement") == 0)
        settingdatanum = 2;
    else if(strcmp(buf, "StorageDefault") == 0)
        settingdatanum = 3;
    else
        return CMPI_RC_ERR_FAILED;

    populate_resource(session, prov_res, sr, settingdatanum);
    xen_sr_free(sr);
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
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    /* Populate the instance's properties with the backend data */
    local_sr_cap_resource *ctx = (local_sr_cap_resource *)resource->ctx;

    //CMSetProperty(inst, "Address",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"Bytes", CMPI_chars);
    //CMSetProperty(inst, "AutomaticAllocation",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "AutomaticDeallocation",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Storage Allocation capabilities", CMPI_chars);
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
    CMSetProperty(inst, "InstanceID",(CMPIValue *)ctx->instanceid, CMPI_chars);
    CMSetProperty(inst, "Limit",(CMPIValue *)&(ctx->limit), CMPI_uint64);
    int mappingBehavior = DMTF_MappingBehavior_Dedicated;
    CMSetProperty(inst, "MappingBehavior",(CMPIValue *)&mappingBehavior, CMPI_uint16);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Parent",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "PoolID",(CMPIValue *)ctx->poolid, CMPI_chars);
    CMSetProperty(inst, "Reservation",(CMPIValue *)&(ctx->reservation), CMPI_uint64);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    int type = DMTF_ResourceType_Storage_Extent;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&type, CMPI_uint16);
    CMSetProperty(inst, "VirtualQuantity",(CMPIValue *)&(ctx->quantity), CMPI_uint64);
    CMSetProperty(inst, "Weight",(CMPIValue *)&(ctx->weight), CMPI_uint32);

    return CMPI_RC_OK;
}

XenInstanceMIStub(Xen_StorageCapabilitiesSettingData)
