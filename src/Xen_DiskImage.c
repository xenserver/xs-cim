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

#include <Xen_Disk.h>
#include "providerinterface.h"

typedef struct {
    xen_vdi vdi;
    xen_vdi_record *vdi_rec;
}local_vdi_resource;

static const char *keys[] = {"SystemName",
    "SystemCreationClassName",
    "CreationClassName",
    "DeviceID"}; 
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
    xen_vdi_set *vdi_set = NULL;
    if (!xen_vdi_get_all(session->xen, &vdi_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = vdi_set;
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
        xen_vdi_set_free((xen_vdi_set *)resources->ctx);
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
    xen_vdi_set *vdi_set = (xen_vdi_set *)resources_list->ctx;
    if (vdi_set == NULL || resources_list->current_resource == vdi_set->size) {
        return CMPI_RC_ERR_NOT_FOUND;
    }
    xen_vdi_record *vdi_rec = NULL;
    if (!xen_vdi_get_record(session->xen, &vdi_rec, vdi_set->contents[resources_list->current_resource])) {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    local_vdi_resource *ctx = calloc(sizeof(local_vdi_resource), 1);
    ctx->vdi = vdi_set->contents[resources_list->current_resource];
    ctx->vdi_rec = vdi_rec;
    vdi_set->contents[resources_list->current_resource] = NULL; /* do not delete this*/
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
    local_vdi_resource *ctx = prov_res->ctx;
    if (ctx) {
        if(ctx->vdi_rec)
            xen_vdi_record_free(ctx->vdi_rec);
        if(ctx->vdi)
            xen_vdi_free(ctx->vdi);
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
    xen_vdi vdi;
    xen_vdi_record *vdi_rec = NULL;

    _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));
    if (!xen_vdi_get_by_uuid(session->xen, &vdi, buf) || 
        !xen_vdi_get_record(session->xen, &vdi_rec, vdi)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    local_vdi_resource *ctx = calloc(sizeof(local_vdi_resource), 1);
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
    CMPIInstance *inst
    )
{
    /* get SR from vdi */
    local_vdi_resource *ctx = (local_vdi_resource *)resource->ctx;
    xen_vdi_record* vdi_rec = ctx->vdi_rec;
    xen_sr_record *sr_rec = NULL;

    if (vdi_rec->sr->is_record)
        sr_rec = vdi_rec->sr->u.record;
    else
        xen_sr_get_record(resource->session->xen, &sr_rec, vdi_rec->sr->u.handle);

    if(!sr_rec) {
        return CMPI_RC_ERR_FAILED;
    }

    /* Key Properties */
    char buf[MAX_INSTANCEID_LEN];
    //_CMPICreateNewDeviceInstanceID(buf, sizeof(buf), host_rec->uuid, resource->vdi_rec->uuid);
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf), sr_rec->uuid, vdi_rec->uuid);
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_DiskImage", CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_StoragePool", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)sr_rec->uuid, CMPI_chars);

    /* Rest of the properties */
    Access access = Access_Read_Write_Supported;
    if (strcmp(sr_rec->content_type, "iso") == 0)
        access = Access_Readable;

    long int free_size = vdi_rec->virtual_size - vdi_rec->physical_utilisation;
    CMSetProperty(inst, "ConsumableBlocks",(CMPIValue *)&free_size, CMPI_uint64);
    int blocksize = 1;
    CMSetProperty(inst, "BlockSize",(CMPIValue *)&blocksize, CMPI_uint64);
    CMSetProperty(inst, "NumberOfBlocks",(CMPIValue *)&vdi_rec->virtual_size, CMPI_uint64);

    CMSetProperty(inst, "Description",(CMPIValue *)vdi_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)vdi_rec->name_label, CMPI_chars);

    CMSetProperty(inst, "Access",(CMPIValue *)&access, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AdditionalAvailability",(CMPIValue *)&arr, CMPI_uint16A);
    DMTF_Availability avail = DMTF_Availability_Running_Full_Power;

    //if (!resource->vdi_rec->currently_attached)
    //    avail = Availability_Off_Line;
    CMSetProperty(inst, "Availability",(CMPIValue *)&avail, CMPI_uint16);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Host Storage Extent that can be used in Virtual Systems", CMPI_chars);
    //CMSetProperty(inst, "ConfigInfo",(CMPIValue *)<value>, CMPI_chars);
    DataOrganization dataOrg = DataOrganization_Unknown;
    CMSetProperty(inst, "DataOrganization",(CMPIValue *)&dataOrg, CMPI_uint16);
    //CMSetProperty(inst, "DataRedundancy",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "DeltaReservation",(CMPIValue *)&<value>, CMPI_uint8);
    CMPIArray *arr = xen_utils_convert_string_string_map_to_CMPIArray(
                         resource->broker, vdi_rec->other_config);
    CMSetProperty(inst, "OtherConfig",(CMPIValue *)&arr, CMPI_charsA);

    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    DMTF_EnabledState eState = DMTF_EnabledState_Enabled;
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&eState, CMPI_uint16);
    //CMSetProperty(inst, "ErrorCleared",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "ErrorDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ErrorMethodology",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ExtentStatus",(CMPIValue *)&arr, CMPI_uint16A);
    DMTF_HealthState hState = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&hState, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "IsBasedOnUnderlyingRedundancy",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "LastErrorCode",(CMPIValue *)&<value>, CMPI_uint32);
    //CMSetProperty(inst, "MaxQuiesceTime",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "Name",(CMPIValue *)vdi_rec->uuid, CMPI_chars);
    NameFormat nameFormat = NameFormat_Other;
    CMSetProperty(inst, "NameFormat",(CMPIValue *)&nameFormat, CMPI_uint16);
    NameNamespace nameNamespace = NameNamespace_Other;
    CMSetProperty(inst, "NameNamespace",(CMPIValue *)&nameNamespace, CMPI_uint16);
    //CMSetProperty(inst, "NoSinglePointOfFailure",(CMPIValue *)&<value>, CMPI_boolean);
    DMTF_OperationalStatus oStatus = DMTF_OperationalStatus_OK;
    CMPIArray *arr2 = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr2, 0, (CMPIValue *)&oStatus, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr2, CMPI_uint16A);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&arr, CMPI_charsA);
    CMSetProperty(inst, "OtherNameFormat",(CMPIValue *)"Xen VDI ID", CMPI_chars);
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
    DMTF_RequestedState rState = DMTF_RequestedState_Unknown;
    CMSetProperty(inst, "RequestedState",(CMPIValue *)&rState, CMPI_uint16);
    //CMSetProperty(inst, "SequentialAccess",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "Status",(CMPIValue *)DMTF_Status_OK, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "StatusInfo",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &host_rec->metrics->u.record->last_updated);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);

    if (!vdi_rec->sr->is_record)
        xen_sr_record_free(sr_rec);

    return CMPI_RC_OK;

}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_DiskImage)

/******************************************************************************
* disk_image_create_ref
*
* This function creates a CIMObjectPath to represent a reference to a
* Disk Image (VDI) object
*
* Returns object path on Success and NULL on failure.
*******************************************************************************/
CMPIObjectPath *disk_image_create_ref(
    const CMPIBroker *broker,
    const char *name_space,
    xen_utils_session *session,
    char* sr_uuid,
    char* vdi_uuid
    )
{
    char buf[MAX_INSTANCEID_LEN];
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf), sr_uuid, vdi_uuid);
    CMPIObjectPath *op = CMNewObjectPath(broker, name_space, "Xen_DiskImage", NULL);
    if(op) {
        CMAddKey(op, "DeviceID",(CMPIValue *)buf, CMPI_chars);
        CMAddKey(op, "CreationClassName",(CMPIValue *)"Xen_DiskImage", CMPI_chars);
        CMAddKey(op, "SystemCreationClassName",(CMPIValue *)"Xen_StoragePool", CMPI_chars);
        CMAddKey(op, "SystemName",(CMPIValue *)sr_uuid, CMPI_chars);
    }
    return op;
}
