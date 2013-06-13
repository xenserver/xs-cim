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
#include <cmpidt.h>
#include <cmpimacs.h>
#include "Xen_Capabilities.h"
#include "xen_utils.h"
#include "Xen_VirtualSystemSettingData.h"
#include "providerinterface.h"

static VSMSMethodSupported g_VSMSSynchronousMethodsSupported[] = 
{
    {"DestroySystem", DestroySystemSupported},
    {"AddResourceSetting", AddResourceSupported},
    {"ModifySystemSettings", ModifySystemSettingsSupported},
    {"RemoveResourceSettings", RemoveResourcesSupported},
    {"ModifyResourceSettings", ModifyResourceSettingsSupported}
};

static VSMSMethodSupported g_VSMSAsynchronousMethodsSupported[] = 
{
    {"DefineSystem", DefineSystemSupported},
    {"AddResourceSettings", AddResourcesSupported}
};

static VSMSIndicationType g_VSMSIndicationTypesSupported[] =
{
    VirtualSystemStateChangeIndicationsSupported
};

static const char *mig_svc_cap_cn = "Xen_VirtualSystemMigrationCapabilities";
static const char *mgmt_svc_cap_cn = "Xen_VirtualSystemManagementCapabilities";
static const char *snap_svc_cap_cn = "Xen_VirtualSystemSnapshotServiceCapabilities";

static const char *srms_svc_cn = "Xen_StoragePoolManagementService";
static const char *mgmt_svc_cn = "Xen_VirtualSystemManagementService";
static const char *swtc_svc_cn = "Xen_VirtualSwitchManagementService";
static const char *mig_svc_cn  = "Xen_VirtualSystemMigrationService";
static const char *snap_svc_cn = "Xen_VirtualSystemSnapshotService";

static const char *svc_keys[] = {"SystemName","SystemCreationClassName","CreationClassName","Name"};
static const char *svc_key_property = "Name";

static const char *svc_cap_keys[] = {"InstanceID"}; 
static const char *svc_cap_key_property = "InstanceID";

/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, classname, "CIM_Service"))
        return svc_key_property;
    else
        return svc_cap_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, classname, "CIM_Service"))
        return svc_keys;
    else
        return svc_cap_keys;
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
    /* We need exactly one fake resource to fill up */
    if (resources_list->current_resource >= 1)
        return CMPI_RC_ERR_NOT_FOUND;
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
static CMPIrc _set_storage_pool_management_service(
    provider_resource *resource, 
    xen_host_record* host_rec,
    CMPIInstance *inst)
{
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("--- Entered set_storage_pool_svc_properties"));

    CMSetProperty(inst, "Caption",(CMPIValue *)"XenServer SR Management Service", CMPI_chars);
    DMTF_CommunicationStatus commStatus = DMTF_CommunicationStatus_Communication_OK;
    CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&commStatus, CMPI_uint16);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_StoragePoolManagementService", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"XenServer SR Management Service", CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)"XenServer SR Management Service", CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)host_rec->uuid, CMPI_chars);

    return CMPI_RC_OK;
}
/******************************************************************************
 * Set the CIM instance for the Virtual Switch Management Service
******************************************************************************/
static CMPIrc _set_virtual_switch_management_service(
    provider_resource *resource, 
    xen_host_record *host_rec,
    CMPIInstance *inst
    )
{
    if (resource == NULL) 
        return CMPI_RC_ERR_FAILED;
    if (CMIsNullObject(inst)) 
        return CMPI_RC_ERR_FAILED;

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_VirtualSwitchManagementService", CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)"Xen Virtual Switch Management Service", CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)host_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"The Xen Service responsible for managing a virtual switch.", CMPI_chars);
    CMSetProperty(inst, "Caption",(CMPIValue *)"The Xen Service responsible for managing a virtual switch.", CMPI_chars);
    return CMPI_RC_OK;
}
/******************************************************************************
 * Set the CIM instance for the Virtual System Migration Service
******************************************************************************/
static CMPIrc _set_virtual_system_migration_service(
    provider_resource *resource, 
    xen_host_record *host_rec,
    CMPIInstance *inst
    )
{
    if (resource == NULL) 
        return CMPI_RC_ERR_FAILED;
    if (CMIsNullObject(inst)) 
        return CMPI_RC_ERR_FAILED;

    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AvailableRequestedStates",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "Caption",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_VirtualSystemMigrationService", CMPI_chars);
    //CMSetProperty(inst, "Description",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "DetailedStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ElementName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "HealthState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "InstanceID",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)"Xen Migration Service", CMPI_chars);
    //CMSetProperty(inst, "OperatingStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryOwnerContact",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryOwnerName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "RequestedState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Started",(CMPIValue *)&<value>, CMPI_boolean);
    DMTF_EnabledState enabled_state = DMTF_EnabledState_Enabled;  // 2 == Enabled
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&enabled_state, CMPI_uint16);
    //CMSetProperty(inst, "StartMode",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Status",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)host_rec->uuid, CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, <time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TransitioningToState",(CMPIValue *)&<value>, CMPI_uint16);

    return CMPI_RC_OK;
}
/******************************************************************************
 * Set the capabilities CIM instance for the Virtual SYstem Migration Service
******************************************************************************/
static CMPIrc _set_virtual_system_management_service(
    provider_resource *resource,
    xen_host_record *host_rec,
    CMPIInstance *instance
    )
{
   if (resource == NULL) 
       return CMPI_RC_ERR_FAILED;
   if (CMIsNullObject(instance)) 
       return CMPI_RC_ERR_FAILED;

   /* Set the CMPIInstance properties from the resource data. */
   CMSetProperty(instance, "CreationClassName",(CMPIValue *)"Xen_VirtualSystemManagementService", CMPI_chars);
   CMSetProperty(instance, "Name",(CMPIValue *)"Xen Hypervisor", CMPI_chars);
   CMSetProperty(instance, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
   CMSetProperty(instance, "SystemName",(CMPIValue *)host_rec->uuid, CMPI_chars);
   bool started = true;
   CMSetProperty(instance, "Started",(CMPIValue *)&started, CMPI_boolean);
   DMTF_EnabledState enabled_state = DMTF_EnabledState_Enabled; // 3 == Disabled
   CMSetProperty(instance, "EnabledState",(CMPIValue *)&enabled_state, CMPI_uint16);

   return CMPI_RC_OK;
}
/******************************************************************************
 * Set the capabilities CIM instance for the Virtual SYstem Migration Service
******************************************************************************/
static CMPIrc _set_virtual_system_snapshot_service(
    provider_resource *resource, 
    xen_host_record *host_rec,
    CMPIInstance *inst
    )
{
    if (resource == NULL) 
        return CMPI_RC_ERR_FAILED;
    if (CMIsNullObject(inst)) 
        return CMPI_RC_ERR_FAILED;

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_VirtualSystemSnapshotService", CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)"Xen Virtual System Snapshot Service", CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)host_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"The Xen Service responsible for managing snapshots of virtual systems", CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AvailableRequestedStates",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Caption",(CMPIValue *)"The Xen Service responsible for managing snapshots of virtual systems", CMPI_chars);
    //CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&<value>, CMPI_uint16);
    return CMPI_RC_OK;
}
/******************************************************************************
 * Set the capabilities CIM instance for the Virtual SYstem Migration Service
******************************************************************************/
static void _set_migration_service_capabilities(
    provider_resource *resource, 
    xen_host_record *host_rec, 
    CMPIInstance *inst)
{
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    int cap = Xen_VirtualSystemMigrationCapabilities_DestinationHostFormatsSupported_IPv4DottedDecimalFormatSupported;
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&cap, CMPI_uint16);
    CMSetProperty(inst, "DestinationHostFormatsSupported",(CMPIValue *)&arr, CMPI_uint16A);
    //arr = CMNewArray(broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AsynchronousMethodsSupported",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual System Migration Capabilities", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"Xen Virtual System Migration Capabilities", CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)host_rec->name_label, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)host_rec->uuid, CMPI_chars);
    arr = CMNewArray(resource->broker, 4, CMPI_uint16, NULL);
    int val =     Xen_VirtualSystemMigrationCapabilities_SynchronousMethodsSupported_MigrateVirtualSystemToHostSupported;
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&val, CMPI_uint16);
    val = Xen_VirtualSystemMigrationCapabilities_SynchronousMethodsSupported_MigrateVirtualSystemToSystemSupported;
    CMSetArrayElementAt(arr, 1, (CMPIValue *)&val, CMPI_uint16);
    val = Xen_VirtualSystemMigrationCapabilities_SynchronousMethodsSupported_CheckVirtualSystemIsMigratableToHostSupported;
    CMSetArrayElementAt(arr, 2, (CMPIValue *)&val, CMPI_uint16);
    val = Xen_VirtualSystemMigrationCapabilities_SynchronousMethodsSupported_CheckVirtualSystemIsMigratableToSystemSupported;
    CMSetArrayElementAt(arr, 3, (CMPIValue *)&val, CMPI_uint16);
    CMSetProperty(inst, "SynchronousMethodsSupported",(CMPIValue *)&arr, CMPI_uint16A);

}
/******************************************************************************
 * Set the capabilities CIM instance for the Virtual SYstem Management Service
******************************************************************************/
static void _set_management_service_capabilities(
    provider_resource *resource, 
    xen_host_record *host_rec, 
    CMPIInstance *inst)
{
    /* Set the CMPIInstance properties from the resource data. */
    if(host_rec) {
        CMSetProperty(inst, "InstanceID", (CMPIValue *)host_rec->uuid, CMPI_chars);
        CMSetProperty(inst, "ElementName", (CMPIValue *)host_rec->name_label, CMPI_chars);
    }
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual System Management Capabilities", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"Xen Virtual System Management Capabilities", CMPI_chars);

    bool supported = true;
    CMSetProperty(inst, "ElementNameEditSupported", (CMPIValue *)&supported, CMPI_boolean);
    CMPIArray *statesSupported = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    int state = 2; /* 'Enabled' */
    CMSetArrayElementAt(statesSupported, 0, (CMPIValue *)&state, CMPI_uint16);
    CMSetProperty(inst, "RequestedStatesSupported", (CMPIValue *)&statesSupported, CMPI_uint16A);
#if 0
    xen_string_set *caps = host_rec->capabilities;
#endif
    CMPIArray *virtSystemTypesSupported = CMNewArray(resource->broker, 2, CMPI_string, NULL);
    CMSetArrayElementAt(virtSystemTypesSupported, 0, HVM_VM_TYPE, CMPI_chars);
    CMSetArrayElementAt(virtSystemTypesSupported, 1, PV_VM_TYPE,  CMPI_chars);
    CMSetProperty(inst, "VirtualSystemTypesSupported", (CMPIValue *)&virtSystemTypesSupported, CMPI_charsA);

    int numMethods = sizeof(g_VSMSSynchronousMethodsSupported)/
         sizeof(g_VSMSSynchronousMethodsSupported[0]);
    int i=0;
    if(numMethods)
    {
        CMPIArray *syncMethodsSupported = CMNewArray(resource->broker, numMethods, CMPI_uint16, NULL);
        for(i=0; i<numMethods; i++)
            CMSetArrayElementAt(syncMethodsSupported, i, (CMPIValue *)&(g_VSMSSynchronousMethodsSupported[i].supportedType), CMPI_uint16);
        CMSetProperty(inst, "SynchronousMethodsSupported", (CMPIValue *)&syncMethodsSupported, CMPI_uint16A);
    }

    numMethods = sizeof(g_VSMSAsynchronousMethodsSupported)/
        sizeof(g_VSMSAsynchronousMethodsSupported[0]);
    if(numMethods)
    {
        CMPIArray *asyncMethodsSupported = CMNewArray(resource->broker, numMethods, CMPI_uint16, NULL);
        for(i=0; i<numMethods; i++)
            CMSetArrayElementAt(asyncMethodsSupported, i, (CMPIValue *)&(g_VSMSAsynchronousMethodsSupported[i].supportedType), CMPI_uint16);
        CMSetProperty(inst, "AsynchronousMethodsSupported",(CMPIValue *)&asyncMethodsSupported, CMPI_uint16A);    
    }

    int numIndications = sizeof(g_VSMSIndicationTypesSupported)/
        sizeof(g_VSMSIndicationTypesSupported[0]);
    if(numIndications)
    {
        CMPIArray *indicationsSupported = CMNewArray(resource->broker, numIndications, CMPI_uint16, NULL);
        for(i=0; i<numIndications; i++)
            CMSetArrayElementAt(indicationsSupported, i, (CMPIValue *)&(g_VSMSIndicationTypesSupported[i]), CMPI_uint16);
        CMSetProperty(inst, "IndicationsSupported", (CMPIValue *)&indicationsSupported, CMPI_uint16A);
    }
}
/******************************************************************************
 * Set the capabilities CIM instance for the Virtual SYstem Snapshots
 * i.e. what kinds of snapshots are supported and so on
******************************************************************************/
static void _set_snapshot_capabilities(
    provider_resource *resource, 
    xen_host_record *host_rec, 
    CMPIInstance *inst)
{
    if(host_rec) {
        CMSetProperty(inst, "InstanceID", (CMPIValue *)host_rec->uuid, CMPI_chars);
        CMSetProperty(inst, "ElementName", (CMPIValue *)host_rec->name_label, CMPI_chars);
    }

    CMSetProperty(inst, "Caption", (CMPIValue *)"Xen Virtual System Snapshot Capabilities", CMPI_chars);
    CMSetProperty(inst, "Description", (CMPIValue *)"Xen Virtual System Snapshot Capabilities", CMPI_chars);

    CMPIArray *typesSupported = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    int typeSupported = Xen_VirtualSystemSnapshotCapabilities_SnapshotTypesEnabled_Full_Snapshot;
    /*BUGBUG - add diff disks when supported */
    CMSetArrayElementAt(typesSupported, 0, (CMPIValue *)&typeSupported, CMPI_uint16);
    CMSetProperty(inst, "SnapshotTypesEnabled", (CMPIValue *)&typesSupported, CMPI_uint16A);

    bool GuestOSNotificationEnabled = false;
    CMSetProperty(inst, "GuestOSNotificationEnabled", (CMPIValue *)&GuestOSNotificationEnabled, CMPI_boolean);

}
/******************************************************************************
 * Set the capabilities CIM instance for the Virtual SYstem Snapshot Service
 * i.e. what kinds of synchronous and asynchronous methods are supported etc.
******************************************************************************/
static void _set_snapshot_service_capabilities(
    provider_resource *resource, 
    xen_host_record *host_rec, 
    CMPIInstance *inst)
{
    if(host_rec->uuid) {
        CMSetProperty(inst, "InstanceID", (CMPIValue *)host_rec->uuid, CMPI_chars);
        CMSetProperty(inst, "ElementName", (CMPIValue *)host_rec->name_label, CMPI_chars);
    }

    int numMethods = sizeof(g_SnapSvcSynchronousMethodsSupported)/
        sizeof(g_SnapSvcSynchronousMethodsSupported[0]);
    int i=0;
    if(numMethods) {
        CMPIArray *syncMethodsSupported = CMNewArray(resource->broker, numMethods, CMPI_uint16, NULL);
        for(i=0; i<numMethods; i++)
            CMSetArrayElementAt(syncMethodsSupported, i, (CMPIValue *)&(g_SnapSvcSynchronousMethodsSupported[i]), CMPI_uint16);
        CMSetProperty(inst, "SynchronousMethodsSupported", (CMPIValue *)&syncMethodsSupported, CMPI_uint16A);
    }

    numMethods = sizeof(g_SnapSvcAsynchronousMethodsSupported)/
        sizeof(g_SnapSvcAsynchronousMethodsSupported[0]);
    if(numMethods) {
        CMPIArray *asyncMethodsSupported = CMNewArray(resource->broker, numMethods, CMPI_uint16, NULL);
        for(i=0; i<numMethods; i++)
            CMSetArrayElementAt(asyncMethodsSupported, i, (CMPIValue *)&(g_SnapSvcAsynchronousMethodsSupported[i]), CMPI_uint16);
        CMSetProperty(inst, "AsynchronousMethodsSupported", (CMPIValue *)&asyncMethodsSupported, CMPI_uint16A);
    }

    int numSnapshotTypes = sizeof(g_SnapSvcSnapshotTypesSupported)/
        sizeof(g_SnapSvcSnapshotTypesSupported[0]);
    if(numSnapshotTypes) {
        CMPIArray *snapshotTypesSupported = CMNewArray(resource->broker, numSnapshotTypes, CMPI_uint16, NULL);
        for(i=0; i<numSnapshotTypes; i++)
            CMSetArrayElementAt(snapshotTypesSupported, i, (CMPIValue *)&(g_SnapSvcSnapshotTypesSupported[i]), CMPI_uint16);
        CMSetProperty(inst, "SnapshotTypesSupported", (CMPIValue *)&snapshotTypesSupported, CMPI_uint16A);
    }
}

static CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    xen_host_record *host_rec = NULL;
    if(!xen_host_get_record(resource->session->xen, &host_rec, resource->session->host))
        return CMPI_RC_ERR_FAILED;

    /* set the appropriate record properties */
    if(strcmp(mgmt_svc_cn, resource->classname) == 0) {
        _set_virtual_system_management_service(resource, host_rec, inst);
    }
    else if(strcmp(mig_svc_cn, resource->classname) == 0) {
        _set_virtual_system_migration_service(resource, host_rec, inst);
    }
    else if(strcmp(srms_svc_cn, resource->classname) == 0) {
        _set_storage_pool_management_service(resource, host_rec, inst);
    }
    else if(strcmp(snap_svc_cn, resource->classname) == 0) {
        _set_virtual_system_snapshot_service(resource, host_rec, inst);
    }
    else if(strcmp(swtc_svc_cn, resource->classname) == 0) {
        _set_virtual_switch_management_service(resource, host_rec, inst);
    }
    else if(strcmp(mig_svc_cap_cn, resource->classname) == 0) {
        _set_migration_service_capabilities(resource, host_rec, inst);
    }
    else if(strcmp(mgmt_svc_cap_cn, resource->classname) == 0) {
        _set_management_service_capabilities(resource, host_rec, inst);
    }
    else if(strcmp(snap_svc_cap_cn, resource->classname) == 0) {
        _set_snapshot_service_capabilities(resource, host_rec, inst);
    }
    else {
        _set_snapshot_capabilities(resource, host_rec, inst);
    }

    if(host_rec)
        xen_host_record_free(host_rec);
    return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_Services)

