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

#include <string.h>
#include <cmpift.h>
#include "cmpimacs.h"
#include "cmpiutil.h"
#include "cmpitrace.h"

#include "RASDs.h"
#include "Xen_Job.h"
#include "Xen_ComputerSystem.h"
#include "Xen_Capabilities.h"
#include "Xen_VirtualSystemSettingData.h"
#include "provider_common.h"
#include "providerinterface.h"
#include "xen_utils.h"

typedef struct _computer_system_resource {
    xen_vm vm;
    xen_vm_record *vm_rec;
    bool free_handle;
}computer_system_resource;

static const char *vm_cn = "Xen_ComputerSystem"; 
static const char *vm_cap_cn = "Xen_ComputerSystemCapabilities"; 
static const char *vssd_base_cn = "Xen_VirtualSystemSettingData";
static const char *tmpl_cn = "Xen_ComputerSystemTemplate"; 
static const char *snpt_cn = "Xen_ComputerSystemSnapshot"; 
static const char *mem_cn = "Xen_Memory"; 
static const char *mem_rasd_cn = "Xen_MemorySettingData";        
static const char *proc_rasd_cn = "Xen_ProcessorSettingData";        

static const char *vm_keys[]        = {"CreationClassName","Name"}; 
static const char *vm_key_property  = "Name";

static const char *vm_cap_keys[] = {"InstanceID"}; 
static const char *vm_cap_key_property = "InstanceID";

static const char *vssd_keys[]      = {"InstanceID","CreationClassName", NULL}; 
static const char *vssd_key_property = "InstanceID";

static const char *mem_keys[] = {"SystemName","SystemCreationClassName","CreationClassName","DeviceID"}; 
static const char *mem_key_property = "DeviceID";

static const char *rasd_keys[] = {"InstanceID"}; 
static const char *rasd_key_property = "InstanceID";

/******************************************************************************
 ************ Provider Specific functions ************************************* 
 *****************************************************************************/
static CMPIrc computer_system_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst);
static CMPIrc computer_setting_data_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst);
static CMPIrc computer_capabilities_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst);
bool _set_available_operations(
    const CMPIBroker *broker,
    xen_vm_record *vm_rec,
    CMPIInstance *inst, 
    char *property_name);
void _state_change_job(void* async_job);

static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, vm_cn, classname))
        return vm_key_property;
    else if (xen_utils_class_is_subclass_of(broker, vm_cap_cn, classname))
        return vm_cap_key_property;
    else if (xen_utils_class_is_subclass_of(broker, vssd_base_cn, classname))
        return vssd_key_property;
    else if (xen_utils_class_is_subclass_of(broker, mem_cn, classname))
        return mem_key_property;
    else 
        return rasd_key_property;

}

static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, vm_cn, classname))
        return vm_keys;
    if (xen_utils_class_is_subclass_of(broker, vm_cap_cn, classname))
        return vm_cap_keys;
    else if (xen_utils_class_is_subclass_of(broker, vssd_base_cn, classname))
        return vssd_keys;
    else if (xen_utils_class_is_subclass_of(broker, mem_cn, classname))
        return mem_keys;
    else
        return rasd_keys;
}
/*****************************************************************************
 * Function to enumerate provider specific resource
 *
 * @param session - handle to a xen_utils_session object
 * @param resources - pointer to the provider_resource_list 
 *   object, the provider specific resource defined above
 *   is a member of this struct
 * @return CMPIrc error codes
 ******************************************************************************/
static CMPIrc xen_resource_list_enum(
    xen_utils_session *session, 
    provider_resource_list *resources 
    )
{
    enum domain_choice choice = vms_only;

    if (strcmp(resources->classname, tmpl_cn) == 0)
        choice = templates_only;
    else if (strcmp(resources->classname, snpt_cn) == 0)
        choice = snapshots_only;
    else if ((strcmp(resources->classname, mem_rasd_cn) == 0) || 
             (strcmp(resources->classname, proc_rasd_cn) == 0))
        choice = all;

    xen_domain_resources *domain_set = NULL;
    if (!xen_utils_get_domain_resources(session, &domain_set, choice))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = domain_set; 
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
    if (resources != NULL && resources->ctx)
        xen_utils_free_domain_resources((xen_domain_resources *)resources->ctx);
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
    int rc;
    xen_vm_record *vm_rec = NULL;
    xen_vm vm = NULL;
    rc = xen_utils_get_next_domain_resource(
        resources_list->session, 
        (xen_domain_resources *)resources_list->ctx, 
        &vm,
        &vm_rec);

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("xen_utils_get_next_domain_resource return Code = %d", rc));
    /* For the case where RC = 0, there are no more objects, and so we should
     * return 'CMPI_RC_ERR_NOT_FOUND' to ensure that the ProxyProvider breaks
     * out of the loop enumerating objects.
     * For the case where RC = -1, we have encountered an error getting a record
     * back for the given reference. This may be because it no longer exists.
     * In this case we should return 'CMPI_RC_ERR_FAILED' to singal a failure.*/
    if (rc == 0)
      return CMPI_RC_ERR_NOT_FOUND;
    if (rc == -1)
        return CMPI_RC_ERR_FAILED;

    computer_system_resource *ctx = calloc(1, sizeof(computer_system_resource));
    ctx->vm = vm;
    ctx->vm_rec = vm_rec;
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
    if (prov_res->ctx) {
        computer_system_resource *ctx = (computer_system_resource *)prov_res->ctx;
        xen_utils_free_domain_resource(NULL, ctx->vm_rec);
        if(ctx->free_handle)
            xen_vm_free(ctx->vm);
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
    char *res_id, /* in */
    xen_utils_session *session, /* in */
    provider_resource *prov_res /* in , out */
    )
{
    xen_vm vm;
    xen_vm_record *vm_rec = NULL;
    char buf[MAX_INSTANCEID_LEN];

    if (xen_utils_class_is_subclass_of(prov_res->broker, prov_res->classname, vm_cn))
        strncpy(buf, res_id, sizeof(buf)/sizeof(buf[0])-1);
    else
        _CMPIStrncpySystemNameFromID(buf, res_id, sizeof(buf)-1);

    if (!xen_vm_get_by_uuid(session->xen, &vm, buf)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    if (!xen_vm_get_record(session->xen, &vm_rec, vm)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    computer_system_resource *ctx = calloc(1, sizeof(computer_system_resource));
    ctx->vm = vm;
    ctx->vm_rec = vm_rec;
    ctx->free_handle = true;
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
static CMPIrc computer_system_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    computer_system_resource *ctx = (computer_system_resource *)resource->ctx;
    xen_vm_record *vm_rec = ctx->vm_rec;
    xen_vm vm = ctx->vm;
    int statusvalue=0, enabledStateVal=0;
    char *statusStr = NULL;
    int requestedStateVal = DMTF_RequestedState_No_Change;
    int healthState = DMTF_HealthState_OK;
    int reset_cap = DMTF_ResetCapability_Other;
    int enabled_default = DMTF_EnabledState_Enabled; /* Enabled */
    xen_utils_session *session = resource->session;
    char buf[MAX_INSTANCEID_LEN];
    memset(&buf, 0, sizeof(buf));

    if ((vm_rec == NULL) || (CMIsNullObject(inst))) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Invalid Parameters"));
        return CMPI_RC_ERR_FAILED;
    }

    /* Key Properties */
    CMSetProperty(inst, "Name",(CMPIValue *)vm_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
    if(resource->ref_only) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Returning just the reference"));
        return CMPI_RC_OK;
    }

    /* Set the CMPIInstance properties from the resource data. */
    if (vm_rec->is_control_domain)
        CMSetProperty(inst, "Caption",(CMPIValue *)"XenServer Host", CMPI_chars);
    else
        CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual Machine", CMPI_chars);

    CMSetProperty(inst, "Description",(CMPIValue *)vm_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)vm_rec->name_label, CMPI_chars);

    //xen_string_string_map *args = xen_string_string_map_alloc(0);
    char *net_bios_name = NULL;

    /* Comment out netbios name support as not consumed. Because it involves as
     * plugin call, this can greatly increase enumeration times.
     *_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("vm_uuid"));
     *xen_utils_add_to_string_string_map("vm_uuid", vm_rec->uuid, &args);
     *_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("About to call plugin"));
     *
     *xen_host_call_plugin(session->xen, &net_bios_name, session->host, "xscim", "read_netbios_name", args);
     *_SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Called Plugin"));
     *xen_string_string_map_free(args);
     *
    */
    CMSetProperty(inst, "NetBiosName",(CMPIValue *)net_bios_name, CMPI_chars);

    /* Set the "Status" and "OperationalStatus" properties of
     * CIM_ManagedSystemElement.  Set the "EnabledState" and
     * "RequestedState property of CIM_EnabledLogicalElement.
     * See those mof files for description of the properties
     * and their values.
     */
    switch (vm_rec->power_state) {
        case XEN_VM_POWER_STATE_HALTED:
            statusvalue = DMTF_OperationalStatus_Dormant;
            enabledStateVal = DMTF_EnabledDefault_Disabled;
            statusStr = DMTF_Status_Stopped;
            break;

        case XEN_VM_POWER_STATE_PAUSED:
            statusvalue = DMTF_OperationalStatus_Dormant;
            enabledStateVal = DMTF_EnabledDefault_Quiesce;
            statusStr = DMTF_Status_Stopped;
            break;

        case XEN_VM_POWER_STATE_RUNNING:
            statusvalue = DMTF_OperationalStatus_OK;
            enabledStateVal = DMTF_EnabledDefault_Enabled;
            statusStr = DMTF_Status_OK;
            break;

        case XEN_VM_POWER_STATE_SUSPENDED:
            statusvalue = DMTF_OperationalStatus_Stopped;
            enabledStateVal = DMTF_EnabledDefault_Enabled_but_Offline;
            statusStr = DMTF_Status_Stopped;
            break;

        default:
            statusvalue = DMTF_OperationalStatus_Unknown;
            enabledStateVal = DMTF_EnabledState_Unknown;
            statusStr = DMTF_Status_No_Contact;
            break;
    }

    CMPIArray *status = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(status, 0, (CMPIValue *)&statusvalue, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&status, CMPI_uint16A);
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&enabledStateVal, CMPI_uint16);
    CMSetProperty(inst, "EnabledDefault", (CMPIValue *)&enabled_default, CMPI_uint16);
    CMSetProperty(inst, "HealthState",(CMPIValue *)&healthState, CMPI_uint16);

    xen_vm_metrics metrics = NULL;
    xen_vm_guest_metrics guest_metrics = NULL;
    if (xen_vm_get_metrics(session->xen, &metrics, vm) && metrics) {
        xen_vm_metrics_record *metrics_rec = NULL;
        if (xen_vm_metrics_get_record(session->xen, &metrics_rec, metrics) && metrics_rec) {
            if (metrics_rec->install_time) {
                CMPIDateTime *install_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, metrics_rec->install_time);
                CMSetProperty(inst, "InstallDate",(CMPIValue *)&install_time, CMPI_dateTime);
            }
            if (metrics_rec->last_updated) {
                CMPIDateTime *last_change_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, metrics_rec->last_updated);
                CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&last_change_time, CMPI_dateTime);
            }
            xen_vm_metrics_record_free(metrics_rec);
        }
        else {
            RESET_XEN_ERROR(resource->session->xen);
        }
        xen_vm_metrics_free(metrics);
    }
    else {
        RESET_XEN_ERROR(resource->session->xen);
    }

    if (xen_vm_get_guest_metrics(session->xen, &guest_metrics, vm) && guest_metrics) {
        xen_vm_guest_metrics_record *guest_metrics_rec = NULL;
        if (xen_vm_guest_metrics_get_record(session->xen, &guest_metrics_rec, guest_metrics) && guest_metrics_rec) {
            char *os_name = NULL, *os_uname = NULL, *major_ver = NULL, *minor_ver = NULL, *distro = NULL;
            int infoCount = 0;
            if ((os_name = xen_utils_get_from_string_string_map(guest_metrics_rec->os_version, "name")))
                infoCount++;
            if ((os_uname  = xen_utils_get_from_string_string_map(guest_metrics_rec->os_version, "uname")))
                infoCount++;
            if ((major_ver = xen_utils_get_from_string_string_map(guest_metrics_rec->os_version, "major")))
                infoCount++;
            if ((minor_ver = xen_utils_get_from_string_string_map(guest_metrics_rec->os_version, "minor")))
                infoCount++;
            if ((distro = xen_utils_get_from_string_string_map(guest_metrics_rec->os_version, "distro")))
                infoCount++;

            CMPIArray* id_info_arr = CMNewArray(resource->broker, infoCount, CMPI_string, NULL);
            CMPIArray* id_desc_arr = CMNewArray(resource->broker, infoCount, CMPI_string, NULL);
            CMPIString *prop=NULL, *val=NULL;
            int propCount = 0;
            if (os_name) {
                prop = CMNewString(resource->broker, "OS Name", NULL);
                CMSetArrayElementAt(id_desc_arr, propCount, (CMPIValue*) &prop, CMPI_string);
                val = CMNewString(resource->broker, os_name, NULL);
                CMSetArrayElementAt(id_info_arr, propCount++, (CMPIValue*) &val, CMPI_string);
            }
            if (os_uname) {
                prop = CMNewString(resource->broker, "OS UName", NULL);
                CMSetArrayElementAt(id_desc_arr, propCount, (CMPIValue*) &prop, CMPI_string);
                val = CMNewString(resource->broker, os_uname, NULL);
                CMSetArrayElementAt(id_info_arr, propCount++, (CMPIValue*) &val, CMPI_string);
            }
            if (major_ver) {
                prop = CMNewString(resource->broker, "Major Version", NULL);
                CMSetArrayElementAt(id_desc_arr, propCount, (CMPIValue*) &prop, CMPI_string);
                val = CMNewString(resource->broker, major_ver, NULL);
                CMSetArrayElementAt(id_info_arr, propCount++, (CMPIValue*) &val, CMPI_string);
            }
            if (minor_ver) {
                prop = CMNewString(resource->broker, "Minor Version", NULL);
                CMSetArrayElementAt(id_desc_arr, propCount, (CMPIValue*) &prop, CMPI_string);
                val = CMNewString(resource->broker, minor_ver, NULL);
                CMSetArrayElementAt(id_info_arr, propCount++, (CMPIValue*) &val, CMPI_string);
            }
            if (distro) {
                prop = CMNewString(resource->broker, "OS Distribution", NULL);
                CMSetArrayElementAt(id_desc_arr, propCount, (CMPIValue*) &prop, CMPI_string);
                val = CMNewString(resource->broker, distro, NULL);
                CMSetArrayElementAt(id_info_arr, propCount++, (CMPIValue*) &val, CMPI_string);
            }
            CMSetProperty(inst, "IdentifyingDescriptions", (CMPIValue *)&id_desc_arr, CMPI_stringA);
            CMSetProperty(inst, "OtherIdentifyingInfo", (CMPIValue *)&id_info_arr, CMPI_stringA);
            xen_vm_guest_metrics_record_free(guest_metrics_rec);
        }
        xen_vm_guest_metrics_free(guest_metrics);
    }
    RESET_XEN_ERROR(resource->session->xen); /* reset any errors */
    _CMPICreateNewSystemInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    _set_available_operations(resource->broker, vm_rec, inst, "AvailableRequestedStates");
    if (vm_rec->resident_on) {
        xen_host_record *host_rec = NULL;
        if (vm_rec->resident_on->is_record)
            host_rec = vm_rec->resident_on->u.record;
        else
            xen_host_get_record(session->xen, &host_rec, vm_rec->resident_on->u.handle);
        if(host_rec) {
            CMSetProperty(inst, "Host", (CMPIValue *)host_rec->uuid, CMPI_chars);
            if (!vm_rec->resident_on->is_record)
                xen_host_record_free(host_rec);
        }
        RESET_XEN_ERROR(resource->session->xen);
    }

    char *owner_name = NULL, *owner_contact = NULL, *vm_roles = NULL;
    if ((owner_name = xen_utils_get_from_string_string_map(vm_rec->other_config, "owner")))
        CMSetProperty(inst, "PrimaryOwnerName",(CMPIValue *)owner_name, CMPI_chars);
    if ((owner_contact = xen_utils_get_from_string_string_map(vm_rec->other_config, "owner_contact")))
        CMSetProperty(inst, "PrimaryOwnerContact",(CMPIValue *)owner_contact, CMPI_chars);
    CMSetProperty(inst, "RequestedState",(CMPIValue *)&requestedStateVal, CMPI_uint16);
    CMSetProperty(inst, "ResetCapability", (CMPIValue *) &reset_cap, CMPI_uint16);
    if ((vm_roles = xen_utils_get_from_string_string_map(vm_rec->other_config, "roles")))
        CMSetProperty(inst, "Roles", (CMPIValue *) vm_roles, CMPI_chars);
    CMSetProperty(inst, "Status",(CMPIValue *)statusStr, CMPI_chars);

    /* FIELDS NOT USED */
    //CMSetProperty(inst, "NameFormat", (CMPIValue *)name_format, CMPI_chars);
    //CMSetProperty(inst, "OtherDedicatedDescriptions", (CMPIValue *)&ded_descs, CMPI_stringA);
    //CMSetProperty(inst, "OtherEnabledState", (CMPIValue *)&other_enabled_state, CMPI_string);
    //CMSetProperty(inst, "StatusDescriptions", (CMPIValue *)status_descs, CMPI_stringA);

    return CMPI_RC_OK;
}
static CMPIrc computer_setting_data_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst)
{
    vssd_from_vm_rec(resource->broker, resource->session, inst, 
                     ((computer_system_resource *)resource->ctx)->vm,
                     ((computer_system_resource *)resource->ctx)->vm_rec,
                     resource->ref_only);
    return CMPI_RC_OK;
}

/* Keep this in sync with the xen operation set */
struct RequestedStateMap VM_Operation_Map[] = {
    {VM_RequestedState_Unknown,     XEN_VM_OPERATIONS_UNDEFINED},
    {VM_RequestedState_Enabled,     XEN_VM_OPERATIONS_START},
    {VM_RequestedState_Disabled,    XEN_VM_OPERATIONS_CLEAN_SHUTDOWN},
    {VM_RequestedState_Shut_Down,   XEN_VM_OPERATIONS_CLEAN_SHUTDOWN},
    {VM_RequestedState_No_Change,   XEN_VM_OPERATIONS_UNDEFINED},
    {VM_RequestedState_Offline,     XEN_VM_OPERATIONS_SUSPEND},
    {VM_RequestedState_Test,        XEN_VM_OPERATIONS_UNDEFINED},
    {VM_RequestedState_Deferred,    XEN_VM_OPERATIONS_UNDEFINED},
    {VM_RequestedState_Quiesce,     XEN_VM_OPERATIONS_PAUSE },
    {VM_RequestedState_Reboot,      XEN_VM_OPERATIONS_CLEAN_REBOOT},
    {VM_RequestedState_Reset,       XEN_VM_OPERATIONS_HARD_REBOOT},
    {VM_RequestedState_Not_Applicable,  XEN_VM_OPERATIONS_UNDEFINED},
    {VM_RequestedState_HardShutdown,XEN_VM_OPERATIONS_HARD_SHUTDOWN},
    {VM_RequestedState_HardReboot,  XEN_VM_OPERATIONS_HARD_REBOOT}
};

/* Find all the available operations on a vm and set the appropriate property */
bool _set_available_operations(
    const CMPIBroker *broker,
    xen_vm_record *vm_rec,
    CMPIInstance *inst, 
    char *property_name
    )
{
        /* Find out all the states supported. We only advertise the onesDMTF cares about */
    int i=0;
    int j;
    int cap_count = 0;
    for (i=0; i<sizeof(VM_Operation_Map)/sizeof(VM_Operation_Map[0]); i++) {
        for(j=0; j<vm_rec->allowed_operations->size; j++) {
            if((VM_Operation_Map[i].xen_val == vm_rec->allowed_operations->contents[j])
               && (VM_Operation_Map[i].xen_val != XEN_VM_OPERATIONS_UNDEFINED)){
                cap_count++;
                break;
            }
        }
    }
    if(cap_count == 0)
        return false;

    CMPIArray *statesSupported = CMNewArray(broker, cap_count, CMPI_uint16, NULL);
    cap_count=0;
    for(i=0; i<sizeof(VM_Operation_Map)/sizeof(VM_Operation_Map[0]); i++) {
        for(j=0; j<vm_rec->allowed_operations->size; j++) {
            if((VM_Operation_Map[i].xen_val == vm_rec->allowed_operations->contents[j]) &&
               (VM_Operation_Map[i].xen_val != XEN_VM_OPERATIONS_UNDEFINED)) {
                CMSetArrayElementAt(statesSupported, cap_count++, (CMPIValue *)&VM_Operation_Map[i].dmtf_val, CMPI_uint16);
                break;
            }
        }
    }
    CMSetProperty(inst, property_name, (CMPIValue *)&statesSupported, CMPI_uint16A);
    return true;
}

static CMPIrc computer_capabilities_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    xen_vm_record *vm_rec = ((computer_system_resource *)resource->ctx)->vm_rec;
    if(vm_rec == NULL)
        return CMPI_RC_ERR_FAILED;

    if(CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;

    /* Set the CMPIInstance properties from the resource data. */
    char buf[MAX_INSTANCEID_LEN];
    memset(&buf, 0, sizeof(buf));
    _CMPICreateNewSystemInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid);
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)vm_rec->name_label, CMPI_chars);

    int nameEditSupported = 0;
    CMSetProperty(inst, "ElementNameEditSupported", (CMPIValue *)&nameEditSupported, CMPI_boolean);

    /* Find out all the states supported. We only advertise the ones DMTF cares about */
    _set_available_operations(resource->broker, vm_rec, inst, "RequestedStatesSupported");

    return CMPI_RC_OK;
}
static CMPIrc memory_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    int prop_val_32=0;
    int64_t prop_val_64=0;
    char buf[MAX_INSTANCEID_LEN];
    memset(&buf, 0, sizeof(buf));

    xen_vm_record* vm_rec = ((computer_system_resource *)resource->ctx)->vm_rec;

    if(vm_rec == NULL)
        return CMPI_RC_ERR_FAILED;

    if(CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;

    int64_t blocks = vm_rec->memory_dynamic_max;

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "CreationClassName", (CMPIValue *)"Xen_Memory", CMPI_chars);

    prop_val_32 = 3; /* RW */
    CMSetProperty(inst, "Access", &prop_val_32, CMPI_uint16);
    /* BlockSize should be set to 1 as per Core/CIM_StorageExtent.mof. */
    uint64_t blocksize = 1;
    CMSetProperty(inst, "BlockSize", (CMPIValue *)&blocksize, CMPI_uint64);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Memory", CMPI_chars);
    prop_val_32 = 2;
    CMSetProperty(inst, "DataOrganization", (CMPIValue *)&prop_val_32, CMPI_uint16);
    prop_val_32 = 1;
    CMSetProperty(inst, "DataRedundancy", (CMPIValue *)&prop_val_32, CMPI_uint16);
    prop_val_32 = 0;
    CMSetProperty(inst, "DeltaReservation", (CMPIValue *)&prop_val_32, CMPI_uint16);
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, "Memory");
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "Description", (CMPIValue *)"Xen Virtual Memory", CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *) vm_rec->name_label, CMPI_chars);
    prop_val_32 = 2; /* Enabled */
    CMSetProperty(inst, "EnabledDefault",(CMPIValue *) &prop_val_32, CMPI_uint16);
    prop_val_32 = 5; /* Not applicable */
    CMSetProperty(inst, "EnabledState",(CMPIValue *) &prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "EndingAddress",(CMPIValue *) &blocks, CMPI_uint64);
    prop_val_32 = 5; /* OK */
    CMSetProperty(inst, "HealthStatus",(CMPIValue *) &blocks, CMPI_uint16);
    prop_val_32 = 0;
    CMSetProperty(inst, "IsBasedOnUnderlyingRedundancy",(CMPIValue *) &prop_val_32, CMPI_boolean);

    CMSetProperty(inst, "Name",(CMPIValue *) vm_rec->uuid, CMPI_chars);
    prop_val_32 = 0;
    CMSetProperty(inst, "NameFormat",(CMPIValue *) &prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "NameNamespace",(CMPIValue *) &prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "NoSinglePointOfFailure",(CMPIValue *) &prop_val_32, CMPI_boolean);
    CMSetProperty(inst, "NumberOfBlocks", (CMPIValue *)&blocks, CMPI_uint64);
    prop_val_32 = 2; /* OK */
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, &prop_val_32, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus", &arr, CMPI_uint16A);
    prop_val_32 = 1;
    CMSetProperty(inst, "Primordial", &prop_val_32, CMPI_boolean);
    CMSetProperty(inst, "Purpose",(CMPIValue *)"Xen Memory", CMPI_chars);
    prop_val_32 = 0;
    CMSetProperty(inst, "SequentialAccess",(CMPIValue *)&prop_val_32, CMPI_boolean);
    prop_val_64 = 0;
    CMSetProperty(inst, "StartingAddress", &prop_val_32, CMPI_uint64);
    CMSetProperty(inst, "Status", (CMPIValue *)"OK", CMPI_chars);
    arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    CMSetArrayElementAt(arr, 0, "OK", CMPI_chars);
    CMSetProperty(inst, "StatusDescriptions", &arr, CMPI_charsA);
    CMSetProperty(inst, "SystemCreationClassName", (CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName", (CMPIValue *)vm_rec->uuid, CMPI_chars);
    prop_val_32 = 1;
    CMSetProperty(inst, "Volatile", (CMPIValue *)&prop_val_32, CMPI_boolean);

    blocks = 0;
    if(vm_rec->metrics->is_record)
        blocks = vm_rec->metrics->u.record->memory_actual;
    else
    {
        xen_vm_metrics_get_memory_actual(resource->session->xen, &blocks, vm_rec->metrics->u.handle);
        RESET_XEN_ERROR(resource->session->xen);
    }
    CMSetProperty(inst, "ConsumableBlocks", (CMPIValue *)&blocks, CMPI_uint64);

    // Following are not used
    //CMSetProperty(inst, "AdditionalAvailability", &arr, CMPI_uint16A);
    //CMSetProperty(inst, "AdditionalErrorData", &prop_val_32, CMPI_uint8);
    //CMSetProperty(inst, "Availability", &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "CorrectableError", &prop_val_32, CMPI_boolean);
    //CMSetProperty(inst, "ErrorAccess",(CMPIValue *) &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "ErrorAddress",(CMPIValue *) &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "ErrorCleared",(CMPIValue *) &prop_val_32, CMPI_boolean);
    //CMSetProperty(inst, "ErrorData",(CMPIValue *) &prop_val_32, CMPI_uint8);
    //CMSetProperty(inst, "ErrorDataOrder",(CMPIValue *) &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "ErrorDescription",(CMPIValue *) &str, CMPI_chars);
    //CMSetProperty(inst, "ErrorInfo",(CMPIValue *) &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "ErrorMethodology",(CMPIValue *) &str, CMPI_chars);
    //CMSetProperty(inst, "ErrorTime",(CMPIValue *) &time, CMPI_dateTime);
    //CMSetProperty(inst, "ErrorTransferSize",(CMPIValue *) &prop_val_32, CMPI_uint32);
    //CMSetProperty(inst, "ExtentStatus",(CMPIValue *) &prop_val_32_arr, CMPI_uint16A);
    //CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *) &str, CMPI_chars);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *) &date, CMPI_dateTime);
    //CMSetProperty(inst, "LastErrorCode",(CMPIValue *) &prop_val_32, CMPI_uint32);
    //CMSetProperty(inst, "LocationIndicator",(CMPIValue *) &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "MaxQuiesceTime",(CMPIValue *) &prop_val_64, CMPI_uint64);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *) &str, CMPI_chars);
    //CMSetProperty(inst, "OtherErrorDescription",(CMPIValue *) &str, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *) &strA, CMPI_stringA);
    //CMSetProperty(inst, "OtherNameFormat",(CMPIValue *) &str, CMPI_chars);
    //CMSetProperty(inst, "OtherNameNamespace",(CMPIValue *) &str, CMPI_chars);
    //CMSetProperty(inst, "PackageRedundancy",(CMPIValue *) &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *) &prop_val_32_arr, CMPI_uint16A);
    //CMSetProperty(inst, "PackageManagementSupported",(CMPIValue *) &prop_val_32, CMPI_boolean);
    //CMSetProperty(inst, "PowerOnHours",(CMPIValue *) &prop_val_64, CMPI_uint64);
    //CMSetProperty(inst, "StatusInfo",(CMPIValue *) &prop_val_32, CMPI_uint16);
    //CMSetProperty(inst, "SystemLevelAddress",(CMPIValue *) &prop_val_32, CMPI_boolean);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *) &time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours",(CMPIValue *) &prop_val_64, CMPI_uint64);

    return CMPI_RC_OK;
}
static CMPIrc memory_rasd_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    return memory_rasd_from_vm_rec(resource->broker, 
                                   resource->session, inst, 
                                   ((computer_system_resource *)resource->ctx)->vm_rec);
}

static CMPIrc proc_rasd_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    return proc_rasd_from_vm_rec(resource->broker, 
                                resource->session, inst, 
                                ((computer_system_resource *)resource->ctx)->vm_rec);
}

static CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    if (xen_utils_class_is_subclass_of(resource->broker, resource->classname, vm_cn))
        return computer_system_set_properties(resource, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, resource->classname, vm_cap_cn))
        return computer_capabilities_set_properties(resource, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, resource->classname, vssd_base_cn))
        return computer_setting_data_set_properties(resource, inst);
    else if(xen_utils_class_is_subclass_of(resource->broker, resource->classname, mem_cn))
        return memory_set_properties(resource, inst);
    else if(xen_utils_class_is_subclass_of(resource->broker, resource->classname, mem_rasd_cn))
        return memory_rasd_set_properties(resource, inst);
    else
        return proc_rasd_set_properties(resource, inst);

    return CMPI_RC_ERR_NOT_FOUND;
}

typedef struct _state_change_job_context {
    xen_vm vm;              /* handle to vm being acted upon */
    int requested_state;    /* state being requested */
}state_change_job_context;

/*******************************************************************************
 * InvokeMethod()
 * Execute an extrinsic method on the specified instance.
 ******************************************************************************/
static CMPIStatus xen_resource_invoke_method(
    CMPIMethodMI * self,                  /* [in] Handle to this provider (i.e. 'self') */
    const CMPIBroker *broker,             /* [in] Broker to handle all CMPI calls */
    const CMPIContext * cmpi_context,     /* [in] Additional context info, if any */
    const CMPIResult * results,           /* [out] Results of this operation */
    const CMPIObjectPath * reference,     /* [in] Contains the CIM namespace, classname and desired object path */
    const char * methodname,              /* [in] Name of the method to apply against the reference object */
    const CMPIArgs * argsin,              /* [in] Method input arguments */
    CMPIArgs * argsout)                   /* [in] Method output arguments */
{
    CMPIStatus status = {CMPI_RC_OK, NULL};      /* Return status of CIM operations. */
    char * nameSpace = CMGetCharPtr(CMGetNameSpace(reference, NULL)); /* Target namespace. */
    char *error_msg = "InvokeMethod on the Xen_ComputerSystem class failed with an unknown error";
    unsigned long rc = DMTF_RequestStateChange_Failed;
    CMPIrc statusrc = CMPI_RC_ERR_FAILED;
    CMPIData argdata;
    xen_utils_session *session = NULL;
    char *uuid = NULL;
    xen_vm vm = NULL;
    struct xen_call_context *ctx = NULL;

    _SBLIM_ENTER("InvokeMethod");
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- self=\"%s\"", self->ft->miName));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- context=\"%s\"", CMGetCharPtr(CDToString(broker, cmpi_context, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- reference=\"%s\"", CMGetCharPtr(CDToString(broker, reference, NULL))));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- methodname=\"%s\"", methodname));
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- namespace=\"%s\"", nameSpace));

    if (!xen_utils_get_call_context(cmpi_context, &ctx, &status))
        goto exit;

    if (!xen_utils_validate_session(&session, ctx)) {
        error_msg = "ERROR: Failed to establish a xen session. Check hostname, username and password.";
        goto exit;
    }

    rc = DMTF_RequestStateChange_Invalid_Parameter;
    argdata = CMGetKey(reference, "Name", &status);
    if ((status.rc != CMPI_RC_OK) || CMIsNullValue(argdata)) {
        error_msg = "ERROR: Couldnt find UUID of the VM to invoke method on, 'Name' parameter is missing";
        goto exit;
    }
    uuid = CMGetCharPtr(argdata.value.string);


    if (strcmp(methodname, "SetupKVPCommunication") == 0) {
      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG, ("Setup KVP"));

      if(xen_utils_setup_kvp_channel(session, uuid) != Xen_KVP_RC_OK){
	goto exit;
      }

      _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Setting up of KVP channel: complete"));
      status.rc = CMPI_RC_OK;
      //Using flag used in exit code.
      rc = DMTF_RequestStateChange_Completed_with_No_Error;
      goto exit;
    }

    /* Only support RequestStateChange() for now. */
    if (strcmp(methodname, "RequestStateChange")) {
        error_msg = "ERROR: Requested Method is not supported";
        rc = DMTF_RequestStateChange_Not_Supported;
        goto exit;
    }

    /* Get RequestedState parameter. */
    int requestedState;
    if (!_GetArgument(broker, argsin, "RequestedState", CMPI_uint16, &argdata, &status)) {
        if (!_GetArgument(broker, argsin, "RequestedState", CMPI_string, &argdata, &status)) {
            error_msg = "ERROR: Couldnt find the 'RequestedState' parameter";
            goto exit;
        }
        requestedState = atoi(CMGetCharPtr(argdata.value.string));
    }
    else
        requestedState = argdata.value.uint16;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("--- RequestedState = \"%d\"", requestedState));

    if (!xen_vm_get_by_uuid(session->xen, &vm, uuid)) {
        error_msg = "ERROR: Couldnt find the specified VM";
        rc = DMTF_RequestStateChange_Failed;
        goto exit;
    }

    CMPIObjectPath *job_instance_op = NULL;
    state_change_job_context *job_ctx = calloc(1, sizeof(state_change_job_context));
    if(!job_ctx)
        goto exit;
    job_ctx->vm = vm;
    job_ctx->requested_state = requestedState;

    if (job_create(broker, cmpi_context, session, "Xen_SystemStateChangeJob", 
                    uuid, _state_change_job, job_ctx, &job_instance_op, &status)) {
        if(job_instance_op) {
            rc = DMTF_RequestStateChange_Method_Parameters_Checked___Job_Started;
            CMAddArg(argsout, "Job", (CMPIValue *)&job_instance_op, CMPI_ref);
        }
    }

exit:
    if (rc != DMTF_RequestStateChange_Completed_with_No_Error && 
        rc != DMTF_RequestStateChange_Method_Parameters_Checked___Job_Started) {
        if(session)
            xen_utils_set_status(broker, &status, statusrc, error_msg, session->xen);
        if (vm)
            xen_vm_free(vm);
    }

    /* Free the resource data. */
    if (session)
        xen_utils_cleanup_session(session);
    if (ctx)
        xen_utils_free_call_context(ctx);

    CMReturnData(results, (CMPIValue *)&rc, CMPI_uint32);
    CMReturnDone(results);

    _SBLIM_RETURNSTATUS(status);
}

void _state_change_job(void* async_job)
{
    Xen_job *job = (Xen_job *)async_job;
    state_change_job_context *job_ctx = (state_change_job_context *)job->job_context;
    xen_vm vm = job_ctx->vm;
    int requestedState = job_ctx->requested_state;
    enum xen_vm_power_state power_state = 0;
    xen_utils_session *session = job->session;
    CMPIrc rc = DMTF_RequestStateChange_Failed;
    char *error_msg = "ERROR: Unknown error", *xen_error = NULL;

    if (!xen_vm_get_power_state(session->xen, &power_state, vm)) {
        error_msg = "ERROR: Couldnt get the specified VM's power state";
        rc = DMTF_RequestStateChange_Failed;
        goto exit;
    }

    int state = JobState_Running;
    job_change_state(job, session, state, 0, 0, NULL);

    /* Move the system to the requested state. */
    if (requestedState == VM_RequestedState_Enabled) {
        /* Can go to Enabled from Defined, Paused, or Suspended states. */
        if (power_state == XEN_VM_POWER_STATE_HALTED) {
            xen_vm_start(session->xen, vm, false
#if XENAPI_VERSION > 400
                ,false
#endif
                );
        }
        else if (power_state == XEN_VM_POWER_STATE_PAUSED)
            xen_vm_unpause(session->xen, vm);
        else if (power_state == XEN_VM_POWER_STATE_SUSPENDED) {
            xen_vm_resume(session->xen, vm, false
#if XENAPI_VERSION > 400
                ,false
#endif
                );
        }
        else {
            error_msg = "ERROR: Invalid state transition. Xen_ComputerSystem specified is already active";
            rc = DMTF_RequestStateChange_Invalid_State_Transition;
        }
    }
    /*
     * We'll treat these state transitions the same.
     */
    else if (requestedState == VM_RequestedState_Disabled || requestedState == VM_RequestedState_Shut_Down) {
        /* Can go to Disabled or Shutdown from Active, Paused, or Suspended states. */
        if (power_state == XEN_VM_POWER_STATE_RUNNING) {
            /* Initiate an asynchronous graceful shutdown of the domain's OS. */
            xen_vm_clean_shutdown(session->xen, vm);
        }
        else if ((power_state == XEN_VM_POWER_STATE_SUSPENDED) ||
            (power_state == XEN_VM_POWER_STATE_PAUSED)) {
            xen_vm_hard_shutdown(session->xen, vm);
        }
        else {
            error_msg = "ERROR: Invalid state transition - Xen_ComputerSystem already disabled";
            rc = DMTF_RequestStateChange_Invalid_State_Transition;
        }
    }
    /*
     * We'll treat these state transitions the same for now.
     */
    else if (requestedState == VM_RequestedState_Reboot) {
        if (power_state == XEN_VM_POWER_STATE_RUNNING) {
            xen_vm_clean_reboot(session->xen, vm);
        }
        else {
            error_msg = "ERROR: Invalid state transition - Xen_ComputerSystem must be active to reboot";
            rc = DMTF_RequestStateChange_Invalid_State_Transition; // "Invalid State Transition"
        }
    }
    else if (requestedState == VM_RequestedState_HardShutdown) { /* Xen defined... Hard shutdown */
        xen_vm_hard_shutdown(session->xen, vm);
    }
    else if (requestedState == VM_RequestedState_Reset || 
        requestedState == VM_RequestedState_HardReboot) { /* Xen defined... Hard reboot */
        xen_vm_hard_reboot(session->xen, vm);
    }
    else if (requestedState == VM_RequestedState_Quiesce) { /* 9 == Quiesce */
        /* Can go to Quiese or Pause from Active state only. */
        if (power_state == XEN_VM_POWER_STATE_RUNNING)
            xen_vm_pause(session->xen, vm);
        else {
            error_msg = "ERROR: Invalid state transition - Xen_ComputerSystem must be active to quiese or pause";
            rc = DMTF_RequestStateChange_Invalid_State_Transition;
        }
    }
    else if (requestedState == VM_RequestedState_Offline) { /* 6 == Offline */
        /* Virtual System Profile says we can go to Suspended from Active
         * or Paused states.  Xen does not allow a Paused domain to be
         * suspended, so we'll enforce Active state.
         */
        if (power_state == XEN_VM_POWER_STATE_RUNNING) {
            xen_vm_suspend(session->xen, vm);
        }
        else {
            error_msg = "ERROR: Invalid state transition - Xen_ComputerSystem must be active to suspend";
            rc = DMTF_RequestStateChange_Invalid_State_Transition;
        }
    }
    else {
        error_msg = "ERROR: Invalid state requested";
        rc = DMTF_RequestStateChange_Invalid_State_Transition;
    }

    if(rc != DMTF_RequestStateChange_Invalid_State_Transition && session->xen->ok) {
        state = JobState_Completed;
        rc = DMTF_RequestStateChange_Completed_with_No_Error;
    }

exit:
    if(rc != DMTF_RequestStateChange_Completed_with_No_Error) {
        state = JobState_Exception;
        if(!session->xen->ok)
            error_msg = xen_error = xen_utils_get_xen_error(session->xen);
    }
    else
        error_msg = "";
    job_change_state(job, session, state, 100, rc, error_msg);

    if(vm)
        xen_vm_free(vm);
    if(job_ctx)
        free(job_ctx);
    if(xen_error)
      free(xen_error);
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_ComputerSystem)

/* Setup the method function table */
XenMethodMIStub(Xen_ComputerSystem)

