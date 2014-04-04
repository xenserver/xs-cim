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

#include "provider_common.h"
#include "providerinterface.h"

#include "RASDs.h"
#include "Xen_Disk.h"

#define XAPI_NULL_REF "OpaqueRef:NULL"

typedef struct {
    xen_vbd vbd;
    xen_vbd_record *vbd_rec;
}vbd_res;

static const char *disk_cn = "Xen_Disk";
static const char *disk_keys[] = {"SystemName","SystemCreationClassName","CreationClassName","DeviceID"}; 
static const char *disk_key_property = "DeviceID";

static const char *disk_drive_cn = "Xen_DiskDrive";

static const char *rasd_cn = "Xen_DiskSettingData";      
static const char *rasd_keys[] = {"InstanceID"}; 
static const char *rasd_key_property = "InstanceID";

static const char *metric_keys[] = {"InstanceID"}; 
static const char *metric_key_property = "InstanceID";

/******************************************************************************
 * Functions to get CIM Class key properties
 *
 * @param broker - CMPI Factory broker
 * @param classname - CIM Classname whose keys are to be retrieved
 * @return char *keys
 *****************************************************************************/
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, disk_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, disk_drive_cn, classname))
        return disk_key_property;
    else if (xen_utils_class_is_subclass_of(broker, rasd_cn, classname))
        return rasd_key_property;
    else
        return metric_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, disk_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, disk_drive_cn, classname))
        return disk_keys;
    else if (xen_utils_class_is_subclass_of(broker, rasd_cn, classname))
        return rasd_keys;
    else
        return metric_keys;
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
    xen_vbd_set *vbd_set = NULL;
    if (!xen_vbd_get_all(session->xen, &vbd_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = vbd_set;
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
        xen_vbd_set_free((xen_vbd_set *)resources->ctx);
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
    provider_resource_list *resources_list, 
    xen_utils_session *session, 
    provider_resource *prov_res
    )
{
    xen_vbd_set *vbd_set = resources_list->ctx;
    while (resources_list->current_resource <= vbd_set->size) {
        if (vbd_set == NULL || resources_list->current_resource == vbd_set->size)
            return CMPI_RC_ERR_NOT_FOUND;
        xen_vbd_record *vbd_rec = NULL;
        if (!xen_vbd_get_record(session->xen, 
                                &vbd_rec, 
                                vbd_set->contents[resources_list->current_resource])) {
            xen_utils_trace_error(session->xen, __FILE__, __LINE__);
            return CMPI_RC_ERR_FAILED;
        }
        /* If we are requesting the CD class filter out the Disks and vice versa */
        /* RASD class is common to both CD and Disks (differeing ResourceTypes) */
        if ((xen_utils_class_is_subclass_of(resources_list->broker, disk_cn, resources_list->classname) 
            && vbd_rec->type == XEN_VBD_TYPE_CD) ||
            (xen_utils_class_is_subclass_of(resources_list->broker, disk_drive_cn, resources_list->classname) 
            && vbd_rec->type == XEN_VBD_TYPE_DISK)  ) {
            xen_vbd_record_free(vbd_rec);
            resources_list->current_resource++; /* Just increment the resource count to get to the next one */
        }
        else {
            vbd_res *ctx = calloc(sizeof(vbd_res), 1);
            ctx->vbd = vbd_set->contents[resources_list->current_resource];
            ctx->vbd_rec = vbd_rec;
            prov_res->ctx = ctx;
            vbd_set->contents[resources_list->current_resource] = NULL; /* do not delete this */
            break;
        }
    }
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
    if (prov_res->ctx) {
        vbd_res *ctx = prov_res->ctx;
        xen_vbd_free(ctx->vbd);
        xen_vbd_record_free(ctx->vbd_rec);
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
    char *res_id,
    xen_utils_session *session,
    provider_resource *prov_res
    )
{
    char buf[MAX_INSTANCEID_LEN];
    xen_vbd vbd = NULL;
    xen_vbd_record *vbd_rec = NULL;
    _CMPIStrncpyDeviceNameFromID(buf, res_id, sizeof(buf));
    if (!xen_vbd_get_by_uuid(session->xen, &vbd, buf) || !xen_vbd_get_record(session->xen, &vbd_rec, vbd)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    /* Skip if we are looking for a Disk class and we found a CD or vice versa */
    if ((xen_utils_class_is_subclass_of(prov_res->broker, disk_cn, prov_res->classname) && (vbd_rec->type == XEN_VBD_TYPE_CD)) ||
        (xen_utils_class_is_subclass_of(prov_res->broker, disk_drive_cn, prov_res->classname) && (vbd_rec->type == XEN_VBD_TYPE_DISK))
        ) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Asking for the wrong type for uuid = %s", buf));
        return CMPI_RC_ERR_NOT_FOUND;
    }
    else {
        vbd_res *ctx = calloc(sizeof(vbd_res), 1);
        ctx->vbd = vbd;
        ctx->vbd_rec = vbd_rec;
        prov_res->ctx = ctx;
        return CMPI_RC_OK;
    }
}
/************************************************************************
 * Function that sets the properties of a CIM object with values from the
 * provider specific resource.
 *
 * @param resource - provider specific resource to get values from
 * @param inst - CIM object whose properties are being set
 * @return CMPIrc return values
*************************************************************************/
CMPIrc disk_set_properties(
    provider_resource *resource, 
    xen_vm_record *vm_rec,
    xen_vdi_record *vdi_rec,
    CMPIInstance *inst
    )
{
    char buf[MAX_INSTANCEID_LEN];
    char *name = NULL, *description= NULL, *caption = NULL, *classname = NULL;
    int access, val_zero = 0;
    int64_t blockSize = 1,virtualSize = 0, consumable_blocks = 0;
    vbd_res *ctx = (vbd_res *)resource->ctx;
    xen_vbd_record *vbd_rec = ctx->vbd_rec;

    if (vdi_rec) {
        virtualSize = vdi_rec->virtual_size;
        consumable_blocks = vdi_rec->virtual_size - vdi_rec->physical_utilisation;
        name = vdi_rec->name_label;
        description = vdi_rec->name_description;
        if (virtualSize < 0)
            virtualSize = 0;
    }
    else {
        name = "No host VDI information available";
        description = "No host VDI information available";
    }
    if (vbd_rec->mode == XEN_VBD_MODE_RW)
        access = Access_Read_Write_Supported; /* Both */
    else
        access = Access_Readable; /* Read Only */

    /* Key properties */
    if (vbd_rec->type == XEN_VBD_TYPE_DISK) {
        classname = "Xen_Disk";
        caption = "Xen Virtual Disk";
    }
    else {
        classname = "Xen_DiskDrive";
        caption = "Xen Virtual CD/DVD";
    }

    CMSetProperty(inst, "CreationClassName", (CMPIValue *)classname, CMPI_chars);
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, vbd_rec->uuid);
    CMSetProperty(inst, "DeviceID", (CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName", (CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName", (CMPIValue *)vm_rec->uuid, CMPI_chars);

    /* From the CIM_LogicalDisk class */
    int availability = DMTF_Availability_Running_Full_Power;
    int enabled_state = DMTF_EnabledState_Enabled;
    int health_state = DMTF_HealthState_OK;
    int enabled_default = DMTF_EnabledDefault_Enabled;
    int op_status = DMTF_OperationalStatus_OK;
    char *status = DMTF_Status_OK;
    if (vbd_rec->currently_attached) {
        availability = DMTF_Availability_Not_Ready;
        enabled_default = DMTF_EnabledDefault_Enabled_but_Offline;
        enabled_state = DMTF_EnabledState_Enabled_but_Offline;
    }
    if (vbd_rec->status_code != 0) {
        op_status = DMTF_OperationalStatus_Error;
        status = DMTF_Status_Error;
    }

    CMSetProperty(inst, "Access",(CMPIValue *)&access, CMPI_uint16);
    CMSetProperty(inst, "Availability", (CMPIValue *)&availability, CMPI_uint16); /* deprecated */
    CMSetProperty(inst, "BlockSize", (CMPIValue *)&blockSize, CMPI_uint64); /* block size is always 1 */
    CMSetProperty(inst, "Caption", (CMPIValue *) caption, CMPI_chars);
    CMSetProperty(inst, "ConsumableBlocks", (CMPIValue *)&consumable_blocks, CMPI_uint64);
    int data_org = DataOrganization_Fixed_Block;
    CMSetProperty(inst, "DataOrganization", (CMPIValue *)&data_org, CMPI_uint16);
    CMSetProperty(inst, "DataRedundancy", (CMPIValue *)&val_zero, CMPI_uint16);
    CMSetProperty(inst, "DeltaReservation", (CMPIValue *)&val_zero, CMPI_uint8);
    CMSetProperty(inst, "Description",(CMPIValue *)description, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *) name, CMPI_chars);
    CMSetProperty(inst, "EnabledDefault", (CMPIValue *)&enabled_default, CMPI_uint16);
    CMSetProperty(inst, "EnabledState", (CMPIValue *)&enabled_state, CMPI_uint16);
    CMSetProperty(inst, "HealthState", (CMPIValue *)&health_state, CMPI_uint16);
    bool redundancy_enabled = false;
    CMSetProperty(inst, "IsBasedOnUnderlyingRedundancy", (CMPIValue *)&redundancy_enabled, CMPI_boolean);
    int location_ind = 4; /* Not supported */
    CMSetProperty(inst, "LocationIndicator", (CMPIValue *)&location_ind, CMPI_uint16);
    CMSetProperty(inst, "Name", (CMPIValue *)vbd_rec->device, CMPI_chars); /* needs to be what the OS sees */
    int name_format = NameFormat_OS_Device_Name;
    int name_namespace = NameNamespace_OS_Device_Namespace; 
    CMSetProperty(inst, "NameFormat", (CMPIValue *)&name_format, CMPI_uint16);
    CMSetProperty(inst, "NameNamespace", (CMPIValue *)&name_namespace, CMPI_uint16);
    bool no_single_pof = false;
    CMSetProperty(inst, "NoSinglePointOfFailure", (CMPIValue *)&no_single_pof, CMPI_boolean);
    CMSetProperty(inst, "NumberOfBlocks", (CMPIValue *)&virtualSize, CMPI_uint64);
    CMPIArray *op_arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(op_arr, 0, (CMPIValue *)&op_status, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus", (CMPIValue *)&op_arr, CMPI_uint16A);
    bool pwr_mgmt_supported = false;
    CMSetProperty(inst, "PowerManagementSupported", (CMPIValue *)&pwr_mgmt_supported, CMPI_boolean);
    bool primordial = true;
    CMSetProperty(inst, "Primordial", (CMPIValue *)&primordial, CMPI_boolean);
    CMSetProperty(inst, "Purpose", (CMPIValue *)"Virtual Disk for a Xen Domain", CMPI_chars);
    int requested_state = DMTF_RequestedState_Not_Applicable; /* Not applicable, state change via RequestStateChange method is not possible */
    CMSetProperty(inst, "RequestedState", (CMPIValue *)&requested_state, CMPI_uint16);
    bool seq_access = false;
    CMSetProperty(inst, "SequentialAccess", (CMPIValue *)&seq_access, CMPI_boolean);
    CMSetProperty(inst, "Status", (CMPIValue *)status, CMPI_chars);
    if (vbd_rec->status_code != 0) {
        CMPIArray *stat_arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
        CMSetArrayElementAt(stat_arr, 0, (CMPIValue *)vbd_rec->status_detail, CMPI_chars);
        CMSetProperty(inst, "StatusDescriptions", (CMPIValue *)&stat_arr, CMPI_charsA);
    }

    /* Not used */
    //CMSetProperty(inst, "AdditionalAvailability", (CMPIValue *)&add_avail, CMPI_uint16A);
    //CMSetProperty(inst, "AvailableRequestedStates", (CMPIValue *)&avail_requested_states, CMPI_uint16A); /* request state change is not supported */
    //CMSetProperty(inst, "ErrorCleared", (CMPIValue *)&error_cleared, CMPI_boolean);
    //CMSetProperty(inst, "ErrorDescription", (CMPIValue *)&error_desc, CMPI_chars);
    //CMSetProperty(inst, "ErrorMethodology", (CMPIValue *)&error_method, CMPI_chars);
    //CMSetProperty(inst, "IdentifyingDescriptions", (CMPIValue *)&id_desc_arr, CMPI_stringA);
    //CMSetProperty(inst, "InstallDate", (CMPIValue *) &install_date, CMPI_dateTime);
    //CMSetProperty(inst, "LastErrorCode", (CMPIValue *) &last_err_code, CMPI_uint32);
    //CMSetProperty(inst, "MaxQuiesceTime" (CMPIValue *)&max_quiesce_time, CMPI_uint64);
    //CMSetProperty(inst, "OtherEnabledState", (CMPIValue *)&other_state, CMPI_chars);
    //CMSetProperty(inst, "OtherEnabledState", (CMPIValue *)&other_id, CMPI_stringA);
    //CMSetProperty(inst, "OtherNameFormat", (CMPIValue *)&other_format, CMPI_chars);
    //CMSetProperty(inst, "OtherNameNamespace", (CMPIValue *)&other_name_namespace, CMPI_chars);
    //CMSetProperty(inst, "PowerManagementCapabilities", (CMPIValue *)&pwr_mgmt_cap_arr, CMPI_uint16A);
    //CMSetProperty(inst, "PowerOnHours", (CMPIValue *)&power_in_hrs, CMPI_uin64);
    //CMSetProperty(inst, "StatusInfo", (CMPIValue *)&status_info, CMPI_uint16);
    //CMSetProperty(inst, "TimeOfLastStateChange", (CMPIValue *)&updated_time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours", (CMPIValue *)&total_power_on_hours, CMPI_uint64);

    return CMPI_RC_OK;
}

CMPIrc disk_metrics_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    xen_vm_record *vm_rec,
    xen_vdi_record *vdi_rec,
    CMPIInstance *inst
    )
{
    char buf[MAX_INSTANCEID_LEN];
    vbd_res *ctx = resource->ctx;
    xen_vbd_record *vbd_rec = ctx->vbd_rec;

    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, vbd_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    // Is this the MetricsDefinitionID or the classname ?
    snprintf(buf, MAX_INSTANCEID_LEN, "%sDef", resource->classname);
    CMSetProperty(inst, "MetricDefinitionId",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "BreakdownDimension",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "BreakdownValue",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual Disk Metrics", CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "Duration",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "MeasuredElementName",(CMPIValue *)vm_rec->name_label, CMPI_chars);

    double io_kbps = 0.0;
    if (strcmp(resource->classname, "Xen_DiskReadThroughput") == 0) {
        snprintf(buf, MAX_INSTANCEID_LEN, "vbd_%s_read", vbd_rec->device);
    }
    else if (strcmp(resource->classname, "Xen_DiskReadLatency") == 0) {
        snprintf(buf, MAX_INSTANCEID_LEN, "vbd_%s_read_latency", vbd_rec->device);
    }
    else if (strcmp(resource->classname, "Xen_DiskWriteLatency") == 0) {
        snprintf(buf, MAX_INSTANCEID_LEN, "vbd_%s_write_latency", vbd_rec->device);
    }
    else
        snprintf(buf, MAX_INSTANCEID_LEN, "vbd_%s_write", vbd_rec->device);

    CMSetProperty(inst, "Description",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)vbd_rec->device, CMPI_chars);
    xen_vm_query_data_source(resource->session->xen, &io_kbps, vbd_rec->vm->u.handle, buf);
    snprintf(buf, MAX_INSTANCEID_LEN, "%f", io_kbps);
    CMSetProperty(inst, "MetricValue", (CMPIValue *)buf, CMPI_chars);
    CMPIDateTime *date_time = xen_utils_CMPIDateTime_now(broker);
    CMSetProperty(inst, "TimeStamp",(CMPIValue *)&date_time, CMPI_dateTime);
    bool vol = true; /* instantaneous metrics */
    CMSetProperty(inst, "Volatile",(CMPIValue *)&vol, CMPI_boolean);

    return CMPI_RC_OK;
}

CMPIrc rasd_set_properties(
    const CMPIBroker *broker,
    xen_utils_session *session, 
    xen_vbd_record* vbd_rec,
    xen_vm_record* vm_rec,
    xen_vdi_record *vdi_rec,
    CMPIInstance *inst
    )
{
    disk_rasd_from_vbd(broker, session, inst, vm_rec, vbd_rec, vdi_rec);
    return CMPI_RC_OK;
}

CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    xen_vm_record *alloced_vm_rec = NULL;
    xen_vm_record *vm_rec = NULL;
    xen_vdi_record *vdi_rec = NULL;
    xen_vdi_record_opt *vdi_opt = NULL;
    vbd_res *ctx = (vbd_res *) resource->ctx;
    xen_vbd_record *vbd_rec = ctx->vbd_rec;
    CMPIrc rc = CMPI_RC_ERR_FAILED;

    if (vbd_rec == NULL || CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;
    xen_vm_record_opt *vm_rec_opt = vbd_rec->vm;

    if (vm_rec_opt->is_record)
        vm_rec = vm_rec_opt->u.record;
    else {
        if (!xen_vm_get_record(resource->session->xen, &vm_rec, vm_rec_opt->u.handle)) {
            xen_utils_trace_error(resource->session->xen, __FILE__, __LINE__);
            goto Exit;
        }
        alloced_vm_rec = vm_rec;
    }

    vdi_opt = vbd_rec->vdi;
    if (vdi_opt && vdi_opt->is_record)
        vdi_rec = vdi_opt->u.record;
    else if (vbd_rec && (strcmp(vbd_rec->vdi->u.handle, "") != 0) && (strcmp(vbd_rec->vdi->u.handle, XAPI_NULL_REF) != 0)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VBD Ref: '%s'", vbd_rec->vdi->u.handle));

        if (!xen_vdi_get_record(resource->session->xen, &vdi_rec, vbd_rec->vdi->u.handle)) {
        /* This can happen if the VDI handle is NULL (such as in an empty CD), just trace it and move on */
        xen_utils_trace_error(resource->session->xen, __FILE__, __LINE__);
        RESET_XEN_ERROR(resource->session->xen);
        }
    } else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("VBD Ref is NULL. Don't call out."));
    }

    /* set the properties on the appropriate classes */
    if (xen_utils_class_is_subclass_of(resource->broker, disk_cn, resource->classname) ||
        xen_utils_class_is_subclass_of(resource->broker, disk_drive_cn, resource->classname))
        rc = disk_set_properties(resource, vm_rec, vdi_rec, inst); /* devcice class */
    else if (xen_utils_class_is_subclass_of(resource->broker, rasd_cn, resource->classname))
        rc = rasd_set_properties(resource->broker, resource->session, vbd_rec, vm_rec, vdi_rec, inst); /* rasd class */
    else
        rc = disk_metrics_set_properties(resource->broker, resource, vm_rec, vdi_rec, inst); /* metrics class */

    Exit:
    if (vdi_opt != NULL && !vdi_opt->is_record)
        xen_vdi_record_free(vdi_rec);
    if (alloced_vm_rec)
        xen_vm_record_free(alloced_vm_rec);
    return rc;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_Disk)

    
