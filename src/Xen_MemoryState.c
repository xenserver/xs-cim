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
#include "Xen_Disk.h"
#include "providerinterface.h"

typedef struct {
    xen_vdi_set *vdi_set;
    xen_vm_set *vm_set;
}local_mem_state_list;

typedef struct {
    xen_vdi vdi;
    xen_vdi_record *vdi_rec;
    xen_vm vm;
} local_mem_state_resource;

//static const char * classname = "Xen_MemoryState";    
static const char *keys[] = {"SystemName","SystemCreationClassName","CreationClassName","DeviceID"}; 
static const char *key_property = "DeviceID";

/*********************************************************
 ************ Provider Specific functions **************** 
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
    xen_vm_set *vm_set = NULL;
    int i;

    xen_vm_set *all_vms = NULL;
    xen_vdi_set *vdi_set = NULL;
    if (!xen_vm_get_all(session->xen, &all_vms)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    for (i=0; i<all_vms->size; i++) {
        xen_vdi vdi = NULL;
        if (xen_vm_get_suspend_vdi(session->xen, 
                                   &vdi, 
                                   all_vms->contents[i]) 
            && vdi) 
        {
            /* BUGBUG: For some reason, I get a NULL VDI for VMs 
             * that dont have a suspend VDI and the call succeeds */
            /* verify if this a NULL VDI by getting its UUID */
            char *uuid = NULL;
            if(xen_vdi_get_uuid(session->xen, &uuid, vdi)) {
                free(uuid);
                ADD_DEVICE_TO_LIST(vdi_set, vdi, xen_vdi);
                ADD_DEVICE_TO_LIST(vm_set, all_vms->contents[i], xen_vm);
            }
            else {
                xen_vdi_free(vdi);
            }
        }
        RESET_XEN_ERROR(session->xen); /* reset error for next iternation */
    }

    /* create a local resource */
    local_mem_state_list *ctx = calloc(sizeof(local_mem_state_list), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    ctx->vdi_set = vdi_set;
    ctx->vm_set = vm_set;
    resources->ctx = ctx;
    xen_vm_set_free(all_vms);

    return CMPI_RC_OK;

Exit:
    if (all_vms)
        xen_vm_set_free(all_vms);
    if (vm_set)
        xen_vm_set_free(vm_set);
    if (vdi_set)
        xen_vdi_set_free(vdi_set);
    return CMPI_RC_ERR_FAILED;
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
    local_mem_state_list *ctx = (local_mem_state_list *)resources->ctx;
    if(ctx) {
        if (ctx->vdi_set)
            xen_vdi_set_free(ctx->vdi_set);
        if (ctx->vm_set)
            xen_vm_set_free(ctx->vm_set);
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
    local_mem_state_list *list = (local_mem_state_list *)resources_list->ctx;
    if (list->vdi_set == NULL || resources_list->current_resource == list->vdi_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_vdi_record *vdi_rec = NULL;
    if (!xen_vdi_get_record(session->xen, 
                            &vdi_rec, 
                            list->vdi_set->contents[resources_list->current_resource])) 
    {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    local_mem_state_resource *ctx = calloc(sizeof(local_mem_state_resource), 1);
    if(ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->vdi = list->vdi_set->contents[resources_list->current_resource];
    ctx->vm  = list->vm_set->contents[resources_list->current_resource];
    ctx->vdi_rec = vdi_rec;
    list->vdi_set->contents[resources_list->current_resource] = NULL; /* do not delete this*/
    list->vm_set->contents[resources_list->current_resource] = NULL;  /* do not delete this*/
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
    local_mem_state_resource *ctx = (local_mem_state_resource *)prov_res->ctx;
    if(ctx) {
        if (ctx->vdi_rec)
            xen_vdi_record_free(ctx->vdi_rec);
        if (ctx->vdi)
            xen_vdi_free(ctx->vdi);
        if (ctx->vm) {
            xen_vm_free(ctx->vm);
        }
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
    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));
    xen_vdi vdi;
    xen_vdi_record *vdi_rec = NULL;
    if (!xen_vdi_get_by_uuid(session->xen, &vdi, buf) || 
        !xen_vdi_get_record(session->xen, &vdi_rec, vdi)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    _CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf));
    xen_vm vm = NULL;
    if (!xen_vm_get_by_uuid(session->xen, &vm, buf)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    local_mem_state_resource *ctx = calloc(sizeof(local_mem_state_resource), 1);
    if(ctx == NULL)
        return CMPI_RC_ERR_FAILED;

    ctx->vm = vm;
    ctx->vdi = vdi;
    ctx->vdi_rec = vdi_rec;
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
    xen_vdi_record *vdi_rec = ((local_mem_state_resource *)resource->ctx)->vdi_rec;
    xen_vm vm = ((local_mem_state_resource *)resource->ctx)->vm;
    char buf[MAX_INSTANCEID_LEN];
    char * uuid = NULL;

    /* Key Properties */
    xen_vm_get_uuid(resource->session->xen, &uuid, vm);
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf), uuid, vdi_rec->uuid);
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_MemoryState", CMPI_chars);

    /* This memory state is associated with a Computer system snapshot */
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_ComputerSystemSnapshot", CMPI_chars);
    _CMPICreateNewSystemInstanceID(buf, sizeof(buf), uuid);
    CMSetProperty(inst, "SystemName",(CMPIValue *)buf, CMPI_chars);

    long long free_size = (vdi_rec->virtual_size - vdi_rec->physical_utilisation);
    CMSetProperty(inst, "ConsumableBlocks",(CMPIValue *)&free_size, CMPI_uint64);
    long long blocksize = 1;
    CMSetProperty(inst, "BlockSize",(CMPIValue *)&blocksize, CMPI_uint64);
    long long size = vdi_rec->virtual_size;
    CMSetProperty(inst, "NumberOfBlocks",(CMPIValue *)&size, CMPI_uint64);

    CMSetProperty(inst, "Description",(CMPIValue *)vdi_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)vdi_rec->name_label, CMPI_chars);

    Access access = Access_Read_Write_Supported;
    CMSetProperty(inst, "Access",(CMPIValue *)&access, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AdditionalAvailability",(CMPIValue *)&arr, CMPI_uint16A);
    DMTF_Availability avail = DMTF_Availability_Quiesced;
    CMSetProperty(inst, "Availability",(CMPIValue *)&avail, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AvailableRequestedStates",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Snapshot of a VM's memory state", CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ClientSettableUsage",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "CreationClassName",(CMPIValue *)<value>, CMPI_chars);
    DataOrganization dataOrg = DataOrganization_Unknown;
    CMSetProperty(inst, "DataOrganization",(CMPIValue *)&dataOrg, CMPI_uint16);

    CMPIArray *arr = xen_utils_convert_string_string_map_to_CMPIArray(
                         resource->broker, vdi_rec->other_config);
    CMSetProperty(inst, "OtherConfig",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "DataRedundancy",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "DeltaReservation",(CMPIValue *)&<value>, CMPI_uint8);
    //CMSetProperty(inst, "Description",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "DetailedStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ElementName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    DMTF_EnabledState eState = DMTF_EnabledState_Enabled;
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&eState, CMPI_uint16);
    //CMSetProperty(inst, "ErrorCleared",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "ErrorDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ErrorMethodology",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ExtentInterleaveDepth",(CMPIValue *)&<value>, CMPI_uint64);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ExtentStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "ExtentStripeLength",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    DMTF_HealthState hState = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&hState, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "InstanceID",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "IsBasedOnUnderlyingRedundancy",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "IsComposite",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "IsConcatenated",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "LastErrorCode",(CMPIValue *)&<value>, CMPI_uint32);
    //CMSetProperty(inst, "LocationIndicator",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "MaxQuiesceTime",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "Name",(CMPIValue *)vdi_rec->uuid, CMPI_chars);
    NameFormat nameFormat = NameFormat_Other;
    CMSetProperty(inst, "NameFormat",(CMPIValue *)&nameFormat, CMPI_uint16);
    NameNamespace nameNamespace = NameNamespace_Other;
    CMSetProperty(inst, "NameNamespace",(CMPIValue *)&nameNamespace, CMPI_uint16);
    //CMSetProperty(inst, "NoSinglePointOfFailure",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "OperatingStatus",(CMPIValue *)&<value>, CMPI_uint16);
    DMTF_OperationalStatus oStatus = DMTF_OperationalStatus_OK;
    arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&oStatus, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherConfig",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&arr, CMPI_charsA);
    CMSetProperty(inst, "OtherNameFormat",(CMPIValue *)"Xen VDI ID", CMPI_chars);
    //CMSetProperty(inst, "OtherNameNamespace",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherUsageDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PackageRedundancy",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "PowerManagementSupported",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "PowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "PrimaryStatus",(CMPIValue *)&<value>, CMPI_uint16);
    bool primordial = false;
    CMSetProperty(inst, "Primordial",(CMPIValue *)&primordial, CMPI_boolean);
    //CMSetProperty(inst, "Purpose",(CMPIValue *)<value>, CMPI_chars);
    DMTF_RequestedState rState = DMTF_RequestedState_Unknown;
    CMSetProperty(inst, "RequestedState",(CMPIValue *)&rState, CMPI_uint16);
    //CMSetProperty(inst, "SequentialAccess",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "Status",(CMPIValue *)DMTF_Status_OK, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "StatusInfo",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "SystemName",(CMPIValue *)<value>, CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "TransitioningToState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Usage",(CMPIValue *)&<value>, CMPI_uint16);

    if (uuid)
        free(uuid);
    return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_MemoryState)


    
