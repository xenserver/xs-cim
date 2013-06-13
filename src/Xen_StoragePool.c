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

#include "Xen_AllocationCapabilities.h"
#include "xen_utils.h"
#include "providerinterface.h"

typedef struct {
    xen_sr sr;
    xen_sr_record *sr_rec;
} local_sr_resource;

static const char * storage_pool_cn = "Xen_StoragePool"; 
static const char * alloc_cap_cn    = "Xen_StorageAllocationCapabilities";        
static const char *keys[]       = {"InstanceID"}; 
static const char *key_property = "InstanceID";

/* Internal functions */
static CMPIrc storage_pool_set_properties(
    provider_resource *resource,
    CMPIInstance *inst);
static CMPIrc allocation_capabilities_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst);
static void get_storage_pool_host(
    xen_utils_session *session, 
    xen_sr_record* sr_rec,
    bool *shared,
    char **host_uuid,
    char **host_name
    );

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
    if (!xen_sr_get_all(session->xen, &sr_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = sr_set;
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
    if (resources && resources->ctx)
        xen_sr_set_free((xen_sr_set *)resources->ctx);
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
    xen_sr_set *sr_set = resources_list->ctx;
    if (sr_set == NULL || resources_list->current_resource >= sr_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_sr_record *sr_rec = NULL;
    if (!xen_sr_get_record(
        session->xen,
        &sr_rec,
        sr_set->contents[resources_list->current_resource]
        )) {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    local_sr_resource *ctx = calloc(1, sizeof(local_sr_resource));
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    ctx->sr = sr_set->contents[resources_list->current_resource];
    sr_set->contents[resources_list->current_resource] = NULL; /* do not delete this*/
    ctx->sr_rec = sr_rec;
    prov_res->ctx = ctx;
    return CMPI_RC_OK;
}

/*****************************************************************************
 * Function to cleanup the resource
 *
 * @param - provider_resource to be freed
 * @return CMPIrc error codes
****************************************************************************/
static CMPIrc xen_resource_record_cleanup(
    provider_resource *prov_res)
{
    if (prov_res->ctx) {
        local_sr_resource *ctx = prov_res->ctx;
        if (ctx->sr_rec)
            xen_sr_record_free(ctx->sr_rec);
        if (ctx->sr)
            xen_sr_free(ctx->sr);
        free(ctx);
    }
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
    if(xen_utils_class_is_subclass_of(prov_res->broker, 
                                      prov_res->classname, 
                                      alloc_cap_cn)) {
        _CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf));
    }
    else
        _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));
    xen_sr sr;
    xen_sr_record *sr_rec = NULL;
    if (!xen_sr_get_by_uuid(session->xen, &sr, buf) || 
        !xen_sr_get_record(session->xen, &sr_rec, sr)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    local_sr_resource *ctx = calloc(1, sizeof(local_sr_resource));
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    ctx->sr = sr;
    ctx->sr_rec = sr_rec;
    prov_res->ctx = ctx;
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
    CMPIInstance *inst)
{
    if (xen_utils_class_is_subclass_of(resource->broker, storage_pool_cn, resource->classname))
        return storage_pool_set_properties(resource, inst); /* pool class */
    else if (xen_utils_class_is_subclass_of(resource->broker, alloc_cap_cn, resource->classname))
        return allocation_capabilities_set_properties(resource, inst); /* allocation capabilities class */
    else
        return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_StoragePool)

/* Internal functions */
static CMPIrc storage_pool_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst)
{
    char *host_uuid = NULL;
    char *host_name = NULL;
    DMTF_OperationalStatus opStatus = DMTF_OperationalStatus_OK;
    char *status = DMTF_Status_OK;
    bool shared = false;
    char buf[MAX_INSTANCEID_LEN];

    local_sr_resource *ctx = resource->ctx; 
    get_storage_pool_host(resource->session, ctx->sr_rec, &shared, &host_uuid, &host_name);
    if(host_uuid){
        _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, host_uuid, ctx->sr_rec->uuid);
        CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    }

    /* Populate the instance's properties with the backend data */
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"Bytes", CMPI_chars);
    CMSetProperty(inst, "Capacity",(CMPIValue *)&(ctx->sr_rec->physical_size), CMPI_uint64);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Storage Repository", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)ctx->sr_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)host_name, CMPI_chars);
    DMTF_HealthState state = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&state, CMPI_uint16);
    /*CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(broker, &<time_value>);
    CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);*/
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)ctx->sr_rec->name_label, CMPI_chars);
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&opStatus, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);

#if XENAPI_VERSION > 400
    CMPIArray *arr2 = xen_utils_convert_string_string_map_to_CMPIArray(
                            resource->broker,
                            ctx->sr_rec->other_config);
    CMSetProperty(inst, "OtherConfig",(CMPIValue *)&arr2, CMPI_charsA);
#endif
    CMSetProperty(inst, "OtherResourceType",(CMPIValue *)ctx->sr_rec->content_type, CMPI_chars);
    CMSetProperty(inst, "PoolID",(CMPIValue *)ctx->sr_rec->uuid, CMPI_chars);
    bool primordial = true;
    CMSetProperty(inst, "Primordial",(CMPIValue *)&primordial, CMPI_boolean);
    CMSetProperty(inst, "Reserved",(CMPIValue *)&ctx->sr_rec->physical_utilisation, CMPI_uint64);
    CMSetProperty(inst, "ResourceSubType",(CMPIValue *)ctx->sr_rec->type, CMPI_chars);
    DMTF_ResourceType res_type = DMTF_ResourceType_Storage_Extent;
    if (strcmp(ctx->sr_rec->content_type, "iso") == 0)
        res_type = DMTF_ResourceType_DVD_drive;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&res_type, CMPI_uint16);

    CMSetProperty(inst, "Shared",(CMPIValue *)&shared, CMPI_boolean);
#if XENAPI_VERSION > 400
    CMPIArray *arr3 = xen_utils_convert_string_string_map_to_CMPIArray(
                            resource->broker,
                            ctx->sr_rec->sm_config);
    CMSetProperty(inst, "SMConfig",(CMPIValue *)&arr3, CMPI_charsA);
#endif
    CMSetProperty(inst, "Status",(CMPIValue *)status, CMPI_chars);
    /*CMPIArray *arr = CMNewArray(broker, 1, CMPI_chars, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);*/

    if (host_name)
        free(host_name);
    if (host_uuid)
        free(host_uuid);
    return CMPI_RC_OK;
}

static CMPIrc allocation_capabilities_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    char buf[MAX_INSTANCEID_LEN];

    local_sr_resource *ctx = resource->ctx;
    /* Populate the instance's properties with the backend data */
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Storage Allocation Capabilities", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *) "Xen Storage Allocation Capabilities", CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)ctx->sr_rec->name_label, CMPI_chars);
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf)/sizeof(buf[0])-1,
        ctx->sr_rec->uuid, "StorageAllocationCapabilities");
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "OtherResourceType",(CMPIValue *)ctx->sr_rec->content_type, CMPI_chars);
    //CMSetProperty(inst, "RequestTypesSupported",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "ResourceSubType",(CMPIValue *)ctx->sr_rec->type, CMPI_chars);
    DMTF_ResourceType res_type = DMTF_ResourceType_Storage_Extent;
    if (strcmp(ctx->sr_rec->content_type, "iso") == 0)
        res_type = DMTF_ResourceType_DVD_drive;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&res_type, CMPI_uint16);
    int sharingMode = DMTF_SharingMode_Dedicated;
    CMSetProperty(inst, "SharingMode",(CMPIValue *)&sharingMode, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "SupportedAddStates",(CMPIValue *)&arr, CMPI_uint16A);
    //CMPIArray *arr = CMNewArray(broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "SupportedRemoveStates",(CMPIValue *)&arr, CMPI_uint16A);

    return CMPI_RC_OK;
}

static void get_storage_pool_host(
    xen_utils_session *session, 
    xen_sr_record* sr_rec,
    bool *shared,
    char **host_uuid,
    char **host_name
    )
{
    xen_host_record *host_rec = NULL;

#define NO_HOST_INFO "Shared"
    /* BUGBUG: The way we infer if an SR is shared is by inspecting the PBD->size. 
    If its > 1, then its an SR shared by multiple hosts */
    if (sr_rec->pbds && sr_rec->pbds->size == 1) {
        *shared = false;
        xen_host host = NULL;
        xen_pbd_record_opt* pbd_opt = sr_rec->pbds->contents[0];
        if (!pbd_opt->is_record) {
            xen_pbd_get_host(session->xen, &host, pbd_opt->u.handle);
            if (host) {
                xen_host_get_record(session->xen, &host_rec, host);
                *host_uuid = strdup(host_rec->uuid);
#if XENAPI_VERSION > 400
                *host_name = strdup(host_rec->hostname);
#endif
                xen_host_free(host);
            }
        }
        else {
            xen_host_record_opt* host_opt = pbd_opt->u.record->host;
            if (host_opt) {
                if (host_opt->is_record)
                    host_rec = host_opt->u.record;
                else
                    xen_host_get_record(session->xen, &host_rec, host_opt->u.handle);
                *host_uuid = strdup(host_rec->uuid);
#if XENAPI_VERSION > 400
                *host_name = strdup(host_rec->hostname);
#endif
            }
        }
    }
    else {
        *shared = true;
        *host_name = strdup(NO_HOST_INFO);
        *host_uuid = strdup(NO_HOST_INFO);
    }
    if (host_rec)
        xen_host_record_free(host_rec);

}

/* External function used by other providers */
CMPIObjectPath * create_storage_pool_ref(
    CMPIBroker *broker, 
    xen_utils_session *session,
    xen_sr_record *sr_rec
    )
{
    char instance_id[MAX_INSTANCEID_LEN];
    char *host_uuid = NULL, *host_name = NULL;
    bool shared = false;

    /* create a storate pool reference and specify the 'System' part of the INstanceID
      as 'Shared' or using a host uuid */
    CMPIObjectPath *result_setting = CMNewObjectPath(broker, DEFAULT_NS, "Xen_StoragePool", NULL);
    if(result_setting) {
        get_storage_pool_host(session, sr_rec, &shared, &host_uuid, &host_name);
        if(host_uuid) {
            _CMPICreateNewDeviceInstanceID(instance_id, MAX_INSTANCEID_LEN, host_uuid, sr_rec->uuid);
            CMAddKey(result_setting, "InstanceID", (CMPIValue *)instance_id, CMPI_chars);
        }
    }

    if (host_uuid)
        free(host_uuid);
    if (host_name)
        free(host_name);

    return result_setting;
}
