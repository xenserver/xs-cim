// Copyright (C) 2008-2009 Citrix Systems Inc
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

#include "providerinterface.h"
#include "Xen_HostComputerSystem.h"
#include "Xen_Job.h"

typedef struct {
    xen_host host;
    xen_host_record *host_rec;
} local_host_resource;

static const char * host_cn = "Xen_HostComputerSystem";         
static const char *host_keys[] = {"CreationClassName","Name"}; 
static const char *host_key_property = "Name";

static const char * hm_cn = "Xen_HostMemory";
static const char *proc_pool_cn = "Xen_ProcessorPool";
static const char *host_cap_cn = "Xen_HostComputerSystemCapabilities";

static const char *hm_keys[] = {"SystemName","SystemCreationClassName","CreationClassName","DeviceID"}; 
static const char *hm_key_property = "DeviceID";
static const char *ac_keys[] = {"InstanceID"}; 
static const char *ac_key_property = "InstanceID";

static const char *proc_alloc_cap_cn = "Xen_ProcessorAllocationCapabilities";        
/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, host_cn, classname))
        return host_key_property;
    else if (xen_utils_class_is_subclass_of(broker, hm_cn, classname))
        return hm_key_property;
    else
        return ac_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, host_cn, classname))
        return host_keys;
    else if (xen_utils_class_is_subclass_of(broker, hm_cn, classname))
        return hm_keys;
    else
        return ac_keys;
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
    if (!xen_host_get_all(session->xen, &host_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = host_set;
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
        xen_host_set_free((xen_host_set *)resources->ctx);
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
    xen_host_set *host_set = (xen_host_set *)resources_list->ctx;
    if (host_set == NULL || resources_list->current_resource == host_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_host_record *host_rec = NULL;
    if (!xen_host_get_record(
        session->xen,
        &host_rec,
        host_set->contents[resources_list->current_resource]
        )) {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    local_host_resource *ctx = calloc(sizeof(local_host_resource), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->host = host_set->contents[resources_list->current_resource];
    ctx->host_rec = host_rec;
    host_set->contents[resources_list->current_resource] = NULL; /* do not delete this*/
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
    provider_resource *prov_res
    )
{
    local_host_resource *ctx = prov_res->ctx;
    if (ctx) {
        if (ctx->host_rec)
            xen_host_record_free(ctx->host_rec);
        if (ctx->host)
            xen_host_free(ctx->host);
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
    xen_host host = NULL;
    xen_host_record *host_rec = NULL;
    char buf[MAX_INSTANCEID_LEN];

    if (xen_utils_class_is_subclass_of(prov_res->broker, host_cn, prov_res->classname))
        /* key property is of the form 'UUID' */
        strncpy(buf, res_uuid, sizeof(buf)-1);
    else
        /* Key property is of the form 'Xen:UUID' */
        _CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf)-1);

    if (!xen_host_get_by_uuid(session->xen, &host, buf) || 
        !xen_host_get_record(session->xen, &host_rec, host)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    local_host_resource *ctx = calloc(sizeof(local_host_resource), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->host = host;
    ctx->host_rec = host_rec;
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
static bool _set_allowed_operations(
    const CMPIBroker *broker,
    xen_host_record *host_rec,
    CMPIInstance *inst,
    char *property_name
    )
{
    int cap_count = 1; /* enabled/disabled is always allowed ? */
    //int i;
    cap_count++; //Default Reboot operation
    cap_count++; //Default shutdown operation
    //if(host_rec->allowed_operations->size > 0) {
      //  for(i=0; i<host_rec->allowed_operations->size; i++) {
        //    if((host_rec->allowed_operations->contents[i] == XEN_HOST_ALLOWED_OPERATIONS_REBOOT) ||
          //     (host_rec->allowed_operations->contents[i] == XEN_HOST_ALLOWED_OPERATIONS_SHUTDOWN))
            //   cap_count++;
       // }
    //}
    CMPIArray *statesSupported = CMNewArray(broker, cap_count, CMPI_uint16, NULL);
    DMTF_RequestedState additional_state;
    cap_count = 0;
    if(host_rec->enabled)
        additional_state = DMTF_RequestedState_Disabled;
    else
        additional_state = DMTF_RequestedState_Enabled;
    CMSetArrayElementAt(statesSupported, cap_count++, (CMPIValue *)&additional_state, CMPI_uint16);
    //for(i=0; i<host_rec->allowed_operations->size; i++) {
      //  if (host_rec->allowed_operations->contents[i] == XEN_HOST_ALLOWED_OPERATIONS_REBOOT) {
        //    additional_state = DMTF_RequestedState_Reboot;
          //  CMSetArrayElementAt(statesSupported, cap_count++, (CMPIValue *)&additional_state, CMPI_uint16);
       // }
       // else if (host_rec->allowed_operations->contents[i] == XEN_HOST_ALLOWED_OPERATIONS_SHUTDOWN) {
         //   additional_state = DMTF_RequestedState_Shut_Down;
           // CMSetArrayElementAt(statesSupported, cap_count++, (CMPIValue *)&additional_state, CMPI_uint16);
       // }
   // }

   //A temp fix for CA-45165 that should be revereted the moment the XAPI call is fixed.
   //It is returning the stated of shutdown and reboot all the time and expecting that in a case where
   //it can't, the error message is appropriate returned.
   additional_state = DMTF_RequestedState_Reboot;
   CMSetArrayElementAt(statesSupported, cap_count++, (CMPIValue *)&additional_state, CMPI_uint16);
   additional_state = DMTF_RequestedState_Shut_Down;
   CMSetArrayElementAt(statesSupported, cap_count++, (CMPIValue *)&additional_state, CMPI_uint16);

    CMSetProperty(inst, property_name, (CMPIValue *)&statesSupported, CMPI_uint16A);
    return true;
}

static CMPIrc host_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    local_host_resource *ctx = (local_host_resource *)resource->ctx;

    /* Key properties to be filled in */
    CMSetProperty(inst, "Name",(CMPIValue *)ctx->host_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);

    xen_host_metrics metrics = NULL;
    xen_host_get_metrics(resource->session->xen, &metrics, ctx->host);
    xen_host_metrics_record *metrics_rec = NULL;
    if (metrics) {
        xen_host_metrics_get_record(resource->session->xen, &metrics_rec, metrics);
        xen_host_metrics_free(metrics);
    }

    /* Populate the instance's properties with the backend data */
    _set_allowed_operations(resource->broker, ctx->host_rec, inst, "AvailableRequestedStates");

    /* Get CN name by calling a plugin on the Host */
    char *CN = NULL;
    xen_string_string_map *args = xen_string_string_map_alloc(0);
    xen_host_call_plugin(resource->session->xen, &CN, ctx->host, "xscim", "read_host_cn", args);
    xen_string_string_map_free(args);
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG,("CN name back from plugin = %s for host %s", CN, ctx->host));
    CMSetProperty(inst, "CN", (CMPIValue *)CN, CMPI_chars);
    if (CN)
      free(CN);
    CMSetProperty(inst, "Caption",(CMPIValue *)"XenServer Host", CMPI_chars);
    DMTF_Dedicated dedicated = DMTF_Dedicated_Other;
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&dedicated, CMPI_uint16);
    CMSetProperty(inst, "Dedicated",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Description",(CMPIValue *)ctx->host_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)ctx->host_rec->name_label, CMPI_chars);
    DMTF_EnabledDefault eDefault = DMTF_EnabledDefault_Enabled;
    CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&eDefault, CMPI_uint16);
    DMTF_EnabledState eState = DMTF_EnabledState_Enabled;
    if (!ctx->host_rec->enabled)
        eState = DMTF_EnabledState_Enabled_but_Offline;
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&eState, CMPI_uint16);
    DMTF_HealthState hState = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&hState, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, <time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    CMSetProperty(inst, "NameFormat",(CMPIValue *)DMTF_NameFormat_Other, CMPI_chars);
    DMTF_OperationalStatus opStatus = DMTF_OperationalStatus_OK;
    if (!ctx->host_rec->enabled)
        opStatus = DMTF_OperationalStatus_Stopped;
    CMPIArray *opStatusArr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(opStatusArr, 0, (CMPIValue *)&opStatus, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&opStatusArr, CMPI_uint16A);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherDedicatedDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    arr = xen_utils_convert_string_string_map_to_CMPIArray(resource->broker, ctx->host_rec->other_config);
    if(arr)
        CMSetProperty(inst, "OtherConfig",(CMPIValue *)&arr, CMPI_stringA);
    CMPIArray *otheridinfo = CMNewArray(resource->broker, 4, CMPI_chars, NULL);
    char* brand = xen_utils_get_from_string_string_map(ctx->host_rec->software_version, "product_brand");
    char* version = xen_utils_get_from_string_string_map(ctx->host_rec->software_version, "product_version");
    char* build = xen_utils_get_from_string_string_map(ctx->host_rec->software_version, "build_number");
    CMSetArrayElementAt(otheridinfo, 0, (CMPIValue *)ctx->host_rec->address, CMPI_chars);
    CMSetArrayElementAt(otheridinfo, 1, (CMPIValue *)brand, CMPI_chars);
    CMSetArrayElementAt(otheridinfo, 2, (CMPIValue *)version, CMPI_chars);
    CMSetArrayElementAt(otheridinfo, 3, (CMPIValue *)build, CMPI_chars);
    CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&otheridinfo, CMPI_charsA);
    CMPIArray *iddesc = CMNewArray(resource->broker, 4, CMPI_chars, NULL);
    CMSetArrayElementAt(iddesc, 0, (CMPIValue *)"IPv4Address", CMPI_chars);
    CMSetArrayElementAt(iddesc, 1, (CMPIValue *)"ProductBrand", CMPI_chars);
    CMSetArrayElementAt(iddesc, 2, (CMPIValue *)"ProductVersion", CMPI_chars);
    CMSetArrayElementAt(iddesc, 3, (CMPIValue *)"BuildNumber", CMPI_chars);
    CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&iddesc, CMPI_charsA);

    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *)&arr, CMPI_uint16A);
    char *owner = xen_utils_get_from_string_string_map(ctx->host_rec->other_config, "owner");
    char *ownercontact = xen_utils_get_from_string_string_map(ctx->host_rec->other_config, "ownercontact");
    if (owner)
        CMSetProperty(inst, "PrimaryOwnerName",(CMPIValue *)owner, CMPI_chars);
    if (ownercontact)
        CMSetProperty(inst, "PrimaryOwnerContact",(CMPIValue *)ownercontact, CMPI_chars);
    DMTF_RequestedState rState = DMTF_RequestedState_Enabled;

    CMSetProperty(inst, "RequestedState",(CMPIValue *)&rState, CMPI_uint16);
    //CMSetProperty(inst, "ResetCapability",(CMPIValue *)&<value>, CMPI_uint16);
    char *serverrole = xen_utils_get_from_string_string_map(ctx->host_rec->other_config, "role");
    if (serverrole) {
        CMPIArray *rolesarr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
        CMSetArrayElementAt(rolesarr, 0, (CMPIValue *)serverrole, CMPI_chars);
        CMSetProperty(inst, "Roles",(CMPIValue *)&rolesarr, CMPI_charsA);
    }

    if (ctx->host_rec->other_config) {
        char *val = xen_utils_get_from_string_string_map(ctx->host_rec->other_config, "boot_time");
        if (val) {
            time_t time = atol(val);
            CMPIDateTime *start_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, time);
            if (start_time) CMSetProperty(inst, "StartTime",(CMPIValue *)&start_time, CMPI_dateTime);
        }
    }
    CMSetProperty(inst, "Status",(CMPIValue *)DMTF_Status_OK, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    if (metrics_rec && (metrics_rec->last_updated != 0)) {
        CMPIDateTime *install_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, metrics_rec->last_updated);
        CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&install_time, CMPI_dateTime);
    }

    /* properties to indicate hosts's time zone. We compute local time zone on this 
    host and assume all hosts are in the same zone since xapi doest report the time zone at this time */
    time_t now = time(NULL);
    struct tm tmnow;
    localtime_r(&now, &tmnow);
    CMSetProperty(inst, "TimeOffset", (CMPIValue *)&tmnow.tm_gmtoff, CMPI_sint32);

    if (metrics_rec)
        xen_host_metrics_record_free(metrics_rec);
    return CMPI_RC_OK;
}

static CMPIrc hostmemory_set_properties(
    provider_resource *resource,
    CMPIInstance *inst
    )
{
    char buf[MAX_INSTANCEID_LEN];
    local_host_resource *ctx = resource->ctx;
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf), ctx->host_rec->uuid, "Memory");
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_HostMemory", CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)ctx->host_rec->uuid, CMPI_chars);

    xen_host_metrics_record *metrics_rec = NULL;
    if (ctx->host_rec->metrics->is_record)
        metrics_rec = ctx->host_rec->metrics->u.record;
    else
        xen_host_metrics_get_record(resource->session->xen, &metrics_rec, ctx->host_rec->metrics->u.handle);

    /* Populate the instance's properties with the backend data */

    //CMSetProperty(inst, "Access",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AdditionalAvailability",(CMPIValue *)&arr, CMPI_uint16A);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint8, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint8);
    //CMSetProperty(inst, "AdditionalErrorData",(CMPIValue *)&arr, CMPI_uint8A);
    //CMSetProperty(inst, "Availability",(CMPIValue *)&<value>, CMPI_uint16);
    uint64_t blockSize = 1;
    CMSetProperty(inst, "BlockSize",(CMPIValue *)&blockSize, CMPI_uint64);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Host Memory", CMPI_chars);
    CMSetProperty(inst, "ConsumableBlocks",(CMPIValue *)&metrics_rec->memory_free, CMPI_uint64);
    //CMSetProperty(inst, "CorrectableError",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "DataOrganization",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "DataRedundancy",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "DeltaReservation",(CMPIValue *)&<value>, CMPI_uint8);
    CMSetProperty(inst, "Description",(CMPIValue *)ctx->host_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)ctx->host_rec->name_label, CMPI_chars);
    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "EnabledState",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "EndingAddress",(CMPIValue *)&metrics_rec->memory_total, CMPI_uint64);
    //CMSetProperty(inst, "ErrorAccess",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ErrorAddress",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "ErrorCleared",(CMPIValue *)&<value>, CMPI_boolean);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint8, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint8);
    //CMSetProperty(inst, "ErrorData",(CMPIValue *)&arr, CMPI_uint8A);
    //CMSetProperty(inst, "ErrorDataOrder",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ErrorDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ErrorInfo",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ErrorMethodology",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ErrorResolution",(CMPIValue *)&<value>, CMPI_uint64);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "ErrorTime",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "ErrorTransferSize",(CMPIValue *)&<value>, CMPI_uint32);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ExtentStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "HealthState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "IsBasedOnUnderlyingRedundancy",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "LastErrorCode",(CMPIValue *)&<value>, CMPI_uint32);
    //CMSetProperty(inst, "MaxQuiesceTime",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "Name",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "NameFormat",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "NameNamespace",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "NoSinglePointOfFailure",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "NumberOfBlocks",(CMPIValue *)&metrics_rec->memory_total, CMPI_uint64);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherErrorDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "OtherNameFormat",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherNameNamespace",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PackageRedundancy",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "PowerManagementSupported",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "PowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    bool primordial = true;
    CMSetProperty(inst, "Primordial",(CMPIValue *)&primordial, CMPI_boolean);
    //CMSetProperty(inst, "Purpose",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "RequestedState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "SequentialAccess",(CMPIValue *)&<value>, CMPI_boolean);
    uint64_t startingAddress = 0;
    CMSetProperty(inst, "StartingAddress",(CMPIValue *)&startingAddress, CMPI_uint64);
    CMSetProperty(inst, "Status",(CMPIValue *)DMTF_Status_OK, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "StatusInfo",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "SystemLevelAddress",(CMPIValue *)&<value>, CMPI_boolean);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "Volatile",(CMPIValue *)&<value>, CMPI_boolean);

    if (!ctx->host_rec->metrics->is_record)
        xen_host_metrics_record_free(metrics_rec);

    return CMPI_RC_OK;
}

static CMPIrc memorypool_set_properties(
    provider_resource *resource,
    CMPIInstance *inst
    )
{
    local_host_resource *ctx = resource->ctx;
    char buf[MAX_INSTANCEID_LEN];
    int prop_val_32;
    xen_host_metrics_record *host_metrics_rec = NULL;

    if (ctx->host_rec->metrics->is_record)
        host_metrics_rec = ctx->host_rec->metrics->u.record;
    else
        xen_host_metrics_get_record(resource->session->xen, &host_metrics_rec, ctx->host_rec->metrics->u.handle);

    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, ctx->host_rec->uuid, "MemoryPool");
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "PoolID", (CMPIValue *)ctx->host_rec->uuid, CMPI_chars);

    int type = DMTF_ResourceType_Memory;
    CMSetProperty(inst, "ResourceType", (CMPIValue *)&type, CMPI_uint16);
    CMSetProperty(inst, "ResourceSubType", (CMPIValue *) "Xen Memory", CMPI_chars);
    CMSetProperty(inst, "AllocationUnits", (CMPIValue *)"Bytes", CMPI_chars);

    CMSetProperty(inst, "Capacity",(CMPIValue *)&(host_metrics_rec->memory_total), CMPI_uint64);
    CMSetProperty(inst, "Caption", (CMPIValue *)"Xen Virtual Memory Pool", CMPI_chars);
    CMSetProperty(inst, "Description", (CMPIValue *)"Xen Virtual Memory Pool", CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)ctx->host_rec->name_label, CMPI_chars);
    prop_val_32 = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState", (CMPIValue *)&prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "InstallDate", (CMPIValue *)installDate, CMPI_dateTime);
    CMSetProperty(inst, "Name", (CMPIValue *)"Xen Virtual Memory Pool", CMPI_chars);
    prop_val_32 = DMTF_OperationalStatus_OK;
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus", (CMPIValue *) &arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherResourceType", (CMPIValue *)other_resource_type, CMPI_String);

    prop_val_32 = 1;
    CMSetProperty(inst, "Primordial" , (CMPIValue *)&prop_val_32, CMPI_boolean);
    //CMSetProperty(inst, "Status", (CMPIValue *)status, CMPI_chars);
    // CMSetProperty(inst, "StatusDescriptions", (CMPIValue *)status_descs, CMPI_chars);

    int64_t reserved = host_metrics_rec->memory_total - host_metrics_rec->memory_free;
    CMSetProperty(inst, "Reserved", (CMPIValue *)&reserved, CMPI_uint64);
    // CMSetProperty(inst, "Unreservable", (CMPIValue *)unreservable, CMPI_uint16);

    if (!ctx->host_rec->metrics->is_record)
        xen_host_metrics_record_free(host_metrics_rec);

    return CMPI_RC_OK;
}

CMPIrc memory_allocation_cap_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst)
{
    char buf[MAX_INSTANCEID_LEN];
    local_host_resource *ctx = resource->ctx;

    /* Populate the instance's properties with the backend data */
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Memory Allocation Capabilities", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"Memory Allocation Capabilities for Xen", CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)ctx->host_rec->name_label, CMPI_chars);
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf)/sizeof(buf[0])-1, ctx->host_rec->uuid, "MemoryAllocationCapabilities");
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "RequestTypesSupported",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    int resourceType = DMTF_ResourceType_Memory;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&resourceType, CMPI_uint16);
    int sharingMode = DMTF_SharingMode_Dedicated;
    CMSetProperty(inst, "SharingMode",(CMPIValue *)&sharingMode, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "SupportedAddStates",(CMPIValue *)&arr, CMPI_uint16A);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "SupportedRemoveStates",(CMPIValue *)&arr, CMPI_uint16A);

    return CMPI_RC_OK;

}
static CMPIrc processorpool_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    CMPIInstance *inst)
{
    uint64_t prop_val_64;
    int prop_val_32;
    char buf[MAX_INSTANCEID_LEN];
    xen_host_record *host_rec = ((local_host_resource *)resource->ctx)->host_rec;

    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, host_rec->uuid, "ProcessorPool");
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "PoolID", (CMPIValue *)host_rec->uuid, CMPI_chars);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("setting ProcessorPool properties -InstanceID: %s", buf));

    prop_val_32 = DMTF_ResourceType_Processor; 
    CMSetProperty(inst, "ResourceType", (CMPIValue *)&prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "ResourceSubType", (CMPIValue *) "xen:vcpu", CMPI_chars);
    CMSetProperty(inst, "AllocationUnits", (CMPIValue *)"count", CMPI_chars);

    /* Capacity is the number of items in xen_host_cpu_set. */
    prop_val_64 = host_rec->host_cpus->size;
    CMSetProperty(inst, "Capacity", (CMPIValue *)&prop_val_64, CMPI_uint64);
    CMSetProperty(inst, "Caption", (CMPIValue *)"Xen Virtual Processor Pool", CMPI_chars);
    CMSetProperty(inst, "Description", (CMPIValue *)host_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)host_rec->name_label, CMPI_chars);
    prop_val_32 = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState", (CMPIValue *)&prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "InstallDate", (CMPIValue *)&installDate, CMPI_dateTime);
    CMSetProperty(inst, "Name", (CMPIValue *)"Xen Virtual Processor Pool", CMPI_chars);

    prop_val_32 = DMTF_OperationalStatus_OK;
    CMPIArray *arr = CMNewArray(broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus", (CMPIValue *) &arr, CMPI_uint16A);

    //CMSetProperty(inst, "OtherResourceType", (CMPIValue *)other_resource_type, CMPI_String);
    prop_val_32 = 1;
    CMSetProperty(inst, "Primordial" , (CMPIValue *)&prop_val_32, CMPI_boolean);
    //CMSetProperty(inst, "Status", (CMPIValue *)status, CMPI_chars);
    // CMSetProperty(inst, "StatusDescriptions", (CMPIValue *)status_descs, CMPI_chars);

    /* Does Xen implement reservation of CPUs by VMs ? */
    prop_val_64 = 0;
    CMSetProperty(inst, "Reserved", (CMPIValue *)&prop_val_64, CMPI_uint64);
    // CMSetProperty(inst, "Unreservable", (CMPIValue *)unreservable, CMPI_uint16);

    return CMPI_RC_OK;
}

static CMPIrc processor_alloc_capabilities_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    CMPIInstance *inst)
{
    char buf[MAX_INSTANCEID_LEN];
    xen_host_record *host_rec = ((local_host_resource *)resource->ctx)->host_rec;

    /* Populate the instance's properties with the backend data */
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Processor Allocation Capabilities", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *) "Xen Processor Allocation Capabilities", CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)host_rec->name_label, CMPI_chars);
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf)/sizeof(buf[0])-1, host_rec->uuid, "ProcessorAllocationCapabilities");
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "RequestTypesSupported",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    int resourceType = DMTF_ResourceType_Processor;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&resourceType, CMPI_uint16);
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
static CMPIrc hostcomputer_capabilities_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    xen_host_record *host_rec = ((local_host_resource *)resource->ctx)->host_rec;
    if(host_rec == NULL)
        return CMPI_RC_ERR_FAILED;

    if(CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;

    /* Set the CMPIInstance properties from the resource data. */
    char buf[MAX_INSTANCEID_LEN];
    _CMPICreateNewSystemInstanceID(buf, MAX_INSTANCEID_LEN, host_rec->uuid);
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)host_rec->name_label, CMPI_chars);

    int nameEditSupported = 0;
    CMSetProperty(inst, "ElementNameEditSupported", (CMPIValue *)&nameEditSupported, CMPI_boolean);

    _set_allowed_operations(broker, host_rec, inst, "RequestedStatesSupported");
    return CMPI_RC_OK;
}

static CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    if (xen_utils_class_is_subclass_of(resource->broker, host_cn, resource->classname))
        return host_set_properties(resource, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, hm_cn, resource->classname))
        return hostmemory_set_properties(resource, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, proc_pool_cn, resource->classname))
        return processorpool_set_properties(resource->broker, resource, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, proc_alloc_cap_cn, resource->classname))
        return processor_alloc_capabilities_set_properties(resource->broker, resource, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, host_cap_cn, resource->classname))
        return hostcomputer_capabilities_set_properties(resource->broker, resource, inst);
    else
        return memorypool_set_properties (resource, inst);
#if 0
    else
        return memory_metric_set_properties(resource, inst);
#endif
}

CMPIObjectPath* host_create_ref(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    xen_host host
    )
{
    char *uuid = NULL;
    CMPIObjectPath *op = NULL;

    if(xen_host_get_uuid(session->xen, &uuid, host) && uuid) {
        op = CMNewObjectPath(broker, DEFAULT_NS, "Xen_HostComputerSystem", NULL);
        CMAddKey(op, "CreationClassName", (CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
        CMAddKey(op, "Name", (CMPIValue *)uuid, CMPI_chars);
        free(uuid);
    }
    return op;
}

/*******************************************************************************
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 ******************************************************************************/
#define MAINTENANCE_MODE_KEY "MAINTENANCE_MODE"
bool _enter_maintenence_mode(
    xen_session *session, 
    xen_host host)
{
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Entering host into Maintenence mode"));
    if(xen_host_disable(session, host)) {
        /* The MAINENANCE_MODE key is required to get XenCenter to display the mode properly */
        xen_host_remove_from_other_config(session, host, MAINTENANCE_MODE_KEY);
        xen_host_add_to_other_config(session, host, MAINTENANCE_MODE_KEY, "true");
        return 1;
    }
    return 0;
}

bool _exit_maintenence_mode(
    xen_session *session, 
    xen_host host)
{
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Exiting host out of Maintenence mode"));
    xen_host_remove_from_other_config(session, host, MAINTENANCE_MODE_KEY);
    if(xen_host_enable(session, host)) {
        /* remove the maintenence mode key from other_config */
        return 1;
    }
    return 0;
}

typedef struct _host_state_change_job_context {
    xen_host host;              /* handle to vm being acted upon */
    int requested_state;        /* state being requested */
}host_state_change_job_context;
static void _state_change_job(void* async_job);

static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,            /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,       /* [in] CMPI factory services */
    const CMPIContext * context,    /* [in] Additional context info, if any */
    const CMPIResult * results,     /* [out] Results of this operation */
    const CMPIObjectPath * reference, /* [in] Contains the CIM namespace, classname and desired object path */
    const char * methodname,        /* [in] Name of the method to apply against the reference object */
    const CMPIArgs * argsin,        /* [in] Method input arguments */
    CMPIArgs * argsout)             /* [in] Method output arguments */
{
    CMPIStatus status = {CMPI_RC_ERR_METHOD_NOT_FOUND, NULL};      /* Return status of CIM operations. */
    unsigned long rc = DMTF_RequestStateChange_Invalid_Parameter;
    CMPIData argdata;
    xen_utils_session * session = NULL;
    char *error_msg = "ERROR: Invalid parameter";
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    xen_host host = NULL;

    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(broker, context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(broker, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\"%s\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));


    struct xen_call_context *ctx = NULL;
    if (!xen_utils_get_call_context(context, &ctx, &status)) {
        error_msg = "ERROR: Couldnt get the caller's credentials";
        goto Exit;
    }
    if (!xen_utils_validate_session(&session, ctx)) {
        error_msg = "ERROR: Couldnt validate caller. Please check the credentials";        
        goto Exit;
    }

    argdata = CMGetKey(reference, "Name", &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Couldnt find UUID of the Host to invoke method on"));
        goto Exit;
    }
    char *res_id = strdup(CMGetCharPtr(argdata.value.string));
    if(!xen_host_get_by_uuid(session->xen, &host, res_id)) {
        error_msg = "ERROR: Could not find the specified host";
        goto Exit;
    }
    /* Invoke the method */
    if (strcmp(methodname, "RequestStateChange") == 0) {
        if (!_GetArgument(broker, argsin, "RequestedState", CMPI_uint16, &argdata, &status)) {
            error_msg = "ERROR: Invalid Parameter";
            goto Exit;
        }
        int state = argdata.value.uint16;
        rc = DMTF_RequestStateChange_Failed;

        CMPIObjectPath *job_instance_op = NULL;
        host_state_change_job_context *job_ctx = calloc(1, sizeof(host_state_change_job_context));
        if(!job_ctx)
            goto Exit;
        job_ctx->host = host;
        job_ctx->requested_state = state;

        if (job_create(broker, context, session, "Xen_SystemStateChangeJob", 
                       res_id, _state_change_job, job_ctx, &job_instance_op, &status)) {
            if(job_instance_op) {
                rc = DMTF_RequestStateChange_Method_Parameters_Checked___Job_Started;
                CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
            }
        }
    }
#if 0
    else if (strcmp(methodname, "SetPowerState") == 0) {
        if (!_GetArgument(broker, argsin, "PowerState", CMPI_uint32, &argdata, &status)) {
            /* return an error */
            goto Exit;
        }
        if (!_GetArgument(broker, argsin, "Time", CMPI_dateTime, &argdata, &status)) {
            /* return an error */
            goto Exit;
        }
    }
#endif
Exit:
    if(rc != DMTF_RequestStateChange_Completed_with_No_Error && 
       rc != DMTF_RequestStateChange_Method_Parameters_Checked___Job_Started) {
        if(session) {
            /* set an appropriate failure message either from xen or ones from above */
            xen_utils_set_status(broker, &status, status.rc,  error_msg, session->xen);
        }
    }

    if (ctx) 
        xen_utils_free_call_context(ctx);
    if (session) 
        xen_utils_cleanup_session(session);

    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);

    _SBLIM_RETURNSTATUS(status);

}
/*
 * Aysnc callback to change the state of the host computer system
 */
static void _state_change_job(void* async_job)
{
    Xen_job *job = (Xen_job *)async_job;
    host_state_change_job_context *job_ctx = (host_state_change_job_context *)job->job_context;
    xen_host host = job_ctx->host;
    int requestedState = job_ctx->requested_state;
    xen_utils_session *session = job->session;
    CMPIrc rc = DMTF_RequestStateChange_Failed;
    char *error_msg = "ERROR: Unknown error", *xen_error = NULL;
    bool enabled = false;
    int jobstate = JobState_Running;

    job_change_state(job, session, jobstate, 0, 0, NULL);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Changing state of host to %d", requestedState));
    if(requestedState == DMTF_RequestedState_Disabled) {
        if(_enter_maintenence_mode(session->xen, host))
            rc = DMTF_RequestStateChange_Completed_with_No_Error;
    }
    if(requestedState == DMTF_RequestedState_Shut_Down) {
        /* Needs to be queiesced before it can be shutdown */
        bool entered_maint_mode = false;
        /* if we arent in maintanence mode, get to it now, since shutdown will fail otherwise */
        if(xen_host_get_enabled(session->xen, &enabled, host) && enabled) {
            _enter_maintenence_mode(session->xen, host);
            entered_maint_mode = true;
        }
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Shutting down host"));
        if(xen_host_shutdown(session->xen, host) && session->xen->ok)
            rc = DMTF_RequestStateChange_Completed_with_No_Error;
        if (entered_maint_mode) {
            /* we want to quietly exit maint mode if we werent in it already */
            session->xen->ok = true; /* need to get out of error state for exit_maint to succeed */
            _exit_maintenence_mode(session->xen, host);
            if(rc != DMTF_RequestStateChange_Completed_with_No_Error)
                session->xen->ok = false; /* get back to error state if we had an error earlier */
        }
    }
    else if(requestedState == DMTF_RequestedState_Enabled) {
        xen_host_allowed_operations_set *allowed_ops = NULL;
        if(xen_host_get_allowed_operations(session->xen, &allowed_ops, host) && allowed_ops) {
            /* host is shut down, restart */
            /* power on works only if the capability exists */
            if(allowed_ops->size == 1 && allowed_ops->contents[0] == XEN_HOST_ALLOWED_OPERATIONS_POWER_ON) {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Powering ON host"));
                if(xen_host_power_on(session->xen, host))
                    rc = DMTF_RequestStateChange_Completed_with_No_Error;
            }
            else {
                if(_exit_maintenence_mode(session->xen, host))
                    rc = DMTF_RequestStateChange_Completed_with_No_Error;
            }
            xen_host_allowed_operations_set_free(allowed_ops);
        }
    }
    else if(requestedState == DMTF_RequestedState_Reboot) {
        /* Needs to be queiesced before it can be rebooted */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Rebooting Host"));
        bool entered_maint_mode = false;
        /* if we arlready arent in maintanence mode, do it now.. reboot fails otherwise */
        if(xen_host_get_enabled(session->xen, &enabled, host) && enabled) {
            entered_maint_mode = true;
            _enter_maintenence_mode(session->xen, host);
        }
        if(xen_host_reboot(session->xen, host) && session->xen->ok)
            rc = DMTF_RequestStateChange_Completed_with_No_Error;
        if(entered_maint_mode) {
            /* if we werent in maintanence mode before this call, get out of it now */
            session->xen->ok = true; /* reset error to get the exit_maint_mode call working */
            _exit_maintenence_mode(session->xen, host);
            if(rc != DMTF_RequestStateChange_Completed_with_No_Error)
                session->xen->ok = false; /* get back to error state */
        }
    }
    else {
        error_msg = "ERROR: Unknown requested state";
    }

    if(rc != DMTF_RequestStateChange_Completed_with_No_Error) {
        jobstate = JobState_Exception;
        if(!session->xen->ok)
            error_msg = xen_error = xen_utils_get_xen_error(session->xen);
    }
    else {
        jobstate = JobState_Completed;
        error_msg = "";
    }

    job_change_state(job, session, jobstate, 100, rc, error_msg);

    if(host)
        xen_host_free(host);
    free(job_ctx);
}


/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_HostComputerSystem)

/* Setup the method function table */
XenMethodMIStub(Xen_HostComputerSystem)

    
