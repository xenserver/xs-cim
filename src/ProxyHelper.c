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
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#include <stdlib.h>
#include "providerinterface.h"

#include "ProxyHelper.h"


/* Provider loader functions defined by all providers */
const XenProviderInstanceFT* Xen_HostPool_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_Services_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_MetricService_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_Job_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_ComputerSystem_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_Processor_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_Console_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_KVP_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_Disk_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_NetworkPort_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_HostComputerSystem_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_HostNetworkPort_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_HostProcessor_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_DiskImage_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_MemoryState_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_StoragePool_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_VirtualSwitch_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_VirtualizationCapabilities_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_MemoryCapabilitiesSettingData_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_NetworkConnectionCapabilitiesSettingData_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_ProcessorCapabilitiesSettingData_Load_Instance_Provider();
const XenProviderInstanceFT* Xen_StorageCapabilitiesSettingData_Load_Instance_Provider();

typedef struct _instance_provider{
    char *classname; /* Name of the class whose provider we need to load */
    const XenProviderInstanceFT *(*provider_load_function)(); /* function to use to load the provider interface */
} xen_instance_provider;

/* A global table of all Xen CIM classnames and instance providers that handle them */
/* Keep in sync with the schema */
xen_instance_provider g_instance_providers[] =  {
    {"Xen_HostPool", Xen_HostPool_Load_Instance_Provider},
    {"Xen_ComputerSystem", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_ComputerSystemSettingData", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_ComputerSystemTemplate", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_ComputerSystemSnapshot", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_VirtualSwitch", Xen_VirtualSwitch_Load_Instance_Provider},
    {"Xen_VirtualSwitchSettingData", Xen_VirtualSwitch_Load_Instance_Provider},
    {"Xen_HostComputerSystem", Xen_HostComputerSystem_Load_Instance_Provider},

    {"Xen_VirtualSystemManagementService", Xen_Services_Load_Instance_Provider},
    {"Xen_VirtualSystemMigrationService", Xen_Services_Load_Instance_Provider},
    {"Xen_VirtualSystemSnapshotService", Xen_Services_Load_Instance_Provider},
    {"Xen_VirtualSwitchManagementService", Xen_Services_Load_Instance_Provider},
    {"Xen_VirtualSwitchManagementService", Xen_Services_Load_Instance_Provider},
    {"Xen_StoragePoolManagementService", Xen_Services_Load_Instance_Provider},

    {"Xen_VirtualSystemManagementServiceJob", Xen_Job_Load_Instance_Provider},
    {"Xen_VirtualSystemMigrationServiceJob", Xen_Job_Load_Instance_Provider},
    {"Xen_ConnectToDiskImageJob", Xen_Job_Load_Instance_Provider},
    {"Xen_DisconnectFromDiskImageJob", Xen_Job_Load_Instance_Provider},
    {"Xen_VirtualSystemModifyResourcesJob", Xen_Job_Load_Instance_Provider},
    {"Xen_VirtualSystemCreateJob", Xen_Job_Load_Instance_Provider},
    {"Xen_SystemStateChangeJob", Xen_Job_Load_Instance_Provider},
    {"Xen_StartSnapshotForestExportJob", Xen_Job_Load_Instance_Provider},
    {"Xen_EndSnapshotForestExportJob", Xen_Job_Load_Instance_Provider},

    {"Xen_HostNetworkPort", Xen_HostNetworkPort_Load_Instance_Provider},
    {"Xen_HostProcessor", Xen_HostProcessor_Load_Instance_Provider},
    {"Xen_HostMemory", Xen_HostComputerSystem_Load_Instance_Provider},
    {"Xen_DiskImage", Xen_DiskImage_Load_Instance_Provider},
    {"Xen_MemoryState", Xen_MemoryState_Load_Instance_Provider},
    {"Xen_HostNetworkPortSettingData", Xen_HostNetworkPort_Load_Instance_Provider},

    {"Xen_ProcessorPool", Xen_HostComputerSystem_Load_Instance_Provider},
    {"Xen_MemoryPool", Xen_HostComputerSystem_Load_Instance_Provider},
    {"Xen_NetworkConnectionPool", Xen_VirtualSwitch_Load_Instance_Provider},
    {"Xen_StoragePool", Xen_StoragePool_Load_Instance_Provider},

    {"Xen_Processor", Xen_Processor_Load_Instance_Provider},
    {"Xen_Memory", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_Disk", Xen_Disk_Load_Instance_Provider},
    {"Xen_DiskDrive", Xen_Disk_Load_Instance_Provider},
    {"Xen_NetworkPort", Xen_NetworkPort_Load_Instance_Provider},
    {"Xen_Console", Xen_Console_Load_Instance_Provider},
    {"Xen_KVP", Xen_KVP_Load_Instance_Provider},
    {"Xen_VirtualSwitchLANEndpoint", Xen_NetworkPort_Load_Instance_Provider},
    {"Xen_ComputerSystemLANEndpoint", Xen_NetworkPort_Load_Instance_Provider},
    {"Xen_VirtualSwitchPort", Xen_NetworkPort_Load_Instance_Provider},
    {"Xen_ProcessorSettingData", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_MemorySettingData", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_DiskSettingData", Xen_Disk_Load_Instance_Provider},
    {"Xen_NetworkPortSettingData", Xen_NetworkPort_Load_Instance_Provider},
    {"Xen_ConsoleSettingData", Xen_Console_Load_Instance_Provider},
    {"Xen_KVPSettingData", Xen_KVP_Load_Instance_Provider},

    {"Xen_MetricService", Xen_MetricService_Load_Instance_Provider},
    {"Xen_HostProcessorUtilization", Xen_HostComputerSystem_Load_Instance_Provider},
    {"Xen_HostNetworkPortReceiveThroughput", Xen_HostNetworkPort_Load_Instance_Provider},
    {"Xen_HostNetworkPortTransmitThroughput", Xen_HostNetworkPort_Load_Instance_Provider},
    {"Xen_ProcessorUtilization", Xen_HostComputerSystem_Load_Instance_Provider},
    {"Xen_DiskReadThroughput", Xen_Disk_Load_Instance_Provider},
    {"Xen_DiskWriteThroughput", Xen_Disk_Load_Instance_Provider},
    {"Xen_DiskReadLatency", Xen_Disk_Load_Instance_Provider},
    {"Xen_DiskWriteLatency", Xen_Disk_Load_Instance_Provider},
    {"Xen_NetworkPortReceiveThroughput", Xen_NetworkPort_Load_Instance_Provider},
    {"Xen_NetworkPortTransmitThroughput", Xen_NetworkPort_Load_Instance_Provider},

    {"Xen_MemoryAllocationCapabilities", Xen_HostComputerSystem_Load_Instance_Provider},
    {"Xen_ProcessorAllocationCapabilities", Xen_HostComputerSystem_Load_Instance_Provider},
    {"Xen_StorageAllocationCapabilities", Xen_StoragePool_Load_Instance_Provider},
    {"Xen_NetworkConnectionAllocationCapabilities", Xen_VirtualSwitch_Load_Instance_Provider},

    {"Xen_MemoryCapabilitiesSettingData", Xen_MemoryCapabilitiesSettingData_Load_Instance_Provider},
    {"Xen_ProcessorCapabilitiesSettingData", Xen_ProcessorCapabilitiesSettingData_Load_Instance_Provider},
    {"Xen_StorageCapabilitiesSettingData", Xen_StorageCapabilitiesSettingData_Load_Instance_Provider},
    {"Xen_NetworkConnectionCapabilitiesSettingData", Xen_NetworkConnectionCapabilitiesSettingData_Load_Instance_Provider},

    {"Xen_VirtualizationCapabilities", Xen_VirtualizationCapabilities_Load_Instance_Provider},
    {"Xen_VirtualSystemManagementCapabilities", Xen_Services_Load_Instance_Provider},
    {"Xen_VirtualSystemMigrationCapabilities", Xen_Services_Load_Instance_Provider},
    {"Xen_VirtualSystemSnapshotServiceCapabilities", Xen_Services_Load_Instance_Provider},
    {"Xen_VirtualSystemSnapshotCapabilities", Xen_Services_Load_Instance_Provider},
    {"Xen_ComputerSystemCapabilities", Xen_ComputerSystem_Load_Instance_Provider},
    {"Xen_HostComputerSystemCapabilities", Xen_HostComputerSystem_Load_Instance_Provider},

};

const XenProviderMethodFT* Xen_VirtualSystemManagementService_Load_Method_Provider();
const XenProviderMethodFT* Xen_Job_Load_Method_Provider();
const XenProviderMethodFT* Xen_VirtualSystemMigrationService_Load_Method_Provider();
const XenProviderMethodFT* Xen_VirtualSystemSnapshotService_Load_Method_Provider();
const XenProviderMethodFT* Xen_VirtualSwitchManagementService_Load_Method_Provider();
const XenProviderMethodFT* Xen_StoragePoolManagementService_Load_Method_Provider();
const XenProviderMethodFT* Xen_ComputerSystem_Load_Method_Provider();
const XenProviderMethodFT* Xen_HostComputerSystem_Load_Method_Provider();
const XenProviderMethodFT* Xen_HostPool_Load_Method_Provider();
const XenProviderMethodFT* Xen_Console_Load_Method_Provider();
const XenProviderMethodFT* Xen_MetricService_Load_Method_Provider();

/* Table that maps the classname and the method providers interfaces that implement them */
/* The following classes are the only ones that implement the InvokeMethod interface */
typedef struct _method_provider{
    char *classname; /* Name of the class whose provider we need to load */
    const XenProviderMethodFT *(*provider_load_function)(); /* function to use to load the provider interface */
} xen_method_provider;

xen_method_provider g_method_providers[] =  {
    {"Xen_VirtualSystemManagementService", Xen_VirtualSystemManagementService_Load_Method_Provider},
    {"Xen_VirtualSystemMigrationService", Xen_VirtualSystemMigrationService_Load_Method_Provider},
    {"Xen_VirtualSystemSnapshotService", Xen_VirtualSystemSnapshotService_Load_Method_Provider},
    {"Xen_VirtualSwitchManagementService", Xen_VirtualSwitchManagementService_Load_Method_Provider},
    {"Xen_StoragePoolManagementService", Xen_StoragePoolManagementService_Load_Method_Provider},
    {"Xen_ComputerSystem", Xen_ComputerSystem_Load_Method_Provider},
    {"Xen_HostComputerSystem", Xen_HostComputerSystem_Load_Method_Provider},
    {"Xen_HostPool", Xen_HostPool_Load_Method_Provider},
    {"Xen_Console", Xen_Console_Load_Method_Provider},
    {"Xen_MetricService", Xen_MetricService_Load_Method_Provider},
    {"Xen_VirtualSystemManagementServiceJob", Xen_Job_Load_Method_Provider},
    {"Xen_VirtualSystemMigrationServiceJob", Xen_Job_Load_Method_Provider},
    {"Xen_VirtualSystemModifyResourcesJob", Xen_Job_Load_Method_Provider},
    {"Xen_VirtualSystemCreateJob", Xen_Job_Load_Method_Provider},
    {"Xen_ConnectToDiskImageJob", Xen_Job_Load_Method_Provider},
    {"Xen_DisconnectFromDiskImageJob", Xen_Job_Load_Method_Provider},
    {"Xen_SystemStateChangeJob", Xen_Job_Load_Method_Provider},
    {"Xen_StartSnapshotForestExportJob", Xen_Job_Load_Method_Provider},
    {"Xen_EndSnapshotForestExportJob", Xen_Job_Load_Method_Provider},

};
/*****************************************************************************
 * Initialize the xen providers
 *
 * @return CMPIrc error codes
 *****************************************************************************/
bool g_bProviderLoaded = false;
CMPIrc prov_pxy_init()
{
    /* Initialized Xen session object. */
    xen_utils_xen_init();
    g_bProviderLoaded = true;
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Unload the provider loader
 *
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_uninit(
    const int terminating
    )
{
    (void)terminating;

    /* Close Xen session object. */
    if(g_bProviderLoaded) {
        /* this is to overcome a bug, where the CIMOM calls unload on the instanceMI
           over and over again, if this provider's methodMI interface returns
           DO_NOT_UNLOAD */
        xen_utils_xen_close();
        g_bProviderLoaded = false;
    }
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Load a bakcned xen provider based on the CIM Classname and initialize its
 * function table
 *
 * @param in broker - CMPI services factory broker
 * @param in classname - CIM classname identifying the xen object
 * @return CMPIrc error codes
 *****************************************************************************/
const XenProviderInstanceFT* prov_pxy_load_xen_instance_provider(
    const CMPIBroker *broker,
    const char *classname
    )
{
    const XenProviderInstanceFT *ft = NULL;
    int i;
    for (i=0; i<sizeof(g_instance_providers)/sizeof(g_instance_providers[0]); i++) {
        if(strcmp(g_instance_providers[i].classname, classname) == 0) {
            ft = g_instance_providers[i].provider_load_function();
        }
    }
    return ft;
}
/*****************************************************************************
 * Enumerates all xen objects identified by the CIM classname 
 *
 * @param in broker - CMPI services factory broker
 * @param in ft - xen backend provider function table
 * @param in classnem - CIM classname identifying the xen object
 * @param in ctx - caller's context
 * @param in properties - properties that the caller is interested in
 * @param out res-list - xen resource list
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_begin(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const char *classname, 
    void *ctx, 
    bool refs_only,
    const char **properties,
    void **res_list
    )
{
    CMPIrc rc = CMPI_RC_OK;
    provider_resource_list *resources = NULL;
    xen_utils_session *session = NULL;
    (void)properties;

    if(res_list == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error res_list = NULL"));
        return CMPI_RC_ERR_FAILED;
    }

    if(!xen_utils_validate_session(&session, ctx)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }
    resources = (provider_resource_list *)calloc(1, sizeof(provider_resource_list));
    if(resources == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Could not allocate memory for resources"));
        return CMPI_RC_ERR_FAILED;
    }
    resources->broker = broker;
    resources->classname = classname;
    resources->session = session;
    resources->ref_only = refs_only;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Begin enumerating %s", classname));

    /* Make Xen call to populate the resources list */
    rc = ft->xen_resource_list_enum(session, resources);
    if(rc != CMPI_RC_OK)  {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error Did not get xen resource list"));       
        goto Error;
    }
    *res_list = (void *)resources;
    return rc;

Error:
    xen_utils_trace_error(session->xen, __FILE__, __LINE__);
    ft->xen_resource_list_cleanup(resources);
    xen_utils_cleanup_session(session);
    if(resources)
        free(resources);

    return CMPI_RC_ERR_FAILED;
}
/*****************************************************************************
 * Cleansup the xen resource list
 *
 * @param in ft - xen backend provider function table
 * @param in res_list - xen resource list
 * @return CMPIrc error codes
 *****************************************************************************/
void prov_pxy_end(
    const XenProviderInstanceFT* ft,
    void *res_list)
{
    provider_resource_list *resources = (provider_resource_list *)res_list;
    if(resources) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("End enumerating %s", resources->classname));
        ft->xen_resource_list_cleanup(resources);
        xen_utils_cleanup_session(resources->session);
        free(resources);
    }
}
/*****************************************************************************
 * Gets the next xen resource from the xen resource list
 *
 * @param in ft - xen backend provider function table
 * @param in res_list - xen resource list
 * @param in properties - CIM properties caller's interested in
 * @param out res - next xen provider resource in the list
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_getnext(
    const XenProviderInstanceFT* ft,
    void *res_list, 
    const char **properties,
    void **res
    )
{
    CMPIrc rc = CMPI_RC_OK;
    provider_resource_list *resources_list = (provider_resource_list *)res_list;
    (void)properties;
    if(resources_list == NULL || res == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                     ("Error getnext:resource_list or res is NULL"));
        return CMPI_RC_ERR_FAILED;
    }

    /* Get the current resource record. */
    RESET_XEN_ERROR(resources_list->session->xen);
    provider_resource *prov_res = calloc(1, sizeof(provider_resource));
    if(prov_res == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error calloc failed"));
        return CMPI_RC_ERR_FAILED;
    }

    /* Copy over the broker and other useful data to the provider's resource */
    prov_res->broker = resources_list->broker;
    prov_res->classname = resources_list->classname;
    prov_res->session = resources_list->session;
    prov_res->ref_only = resources_list->ref_only;
    prov_res->cleanupsession = false;
    rc = ft->xen_resource_record_getnext(resources_list, resources_list->session, prov_res);
    if(rc != CMPI_RC_OK) {
      if(rc != CMPI_RC_ERR_NOT_FOUND) {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Error getnext OK not received "));
	    resources_list->current_resource++; /*Failure to retrieve this record - continue anyway */
      }
        ft->xen_resource_record_cleanup(prov_res);
        free(prov_res);
        return rc;
    }
    resources_list->current_resource++; /*increment the resource index for the next round */
    *res = (void *)prov_res;
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Gets a specific xen resource identified by the CIM Instance passed in
 *
 * @param in broker - CMPI Broker services
 * @param in ft - xen backend provider function table
 * @param in res_id - resource identifying xen object being requested
 * @param in caller_id - CIM Caller's credentials
 * @param in properties - CIM properties caller's interested in
 * @param out res - xen provider resource
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_get(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id, 
    struct xen_call_context * caller_id, 
    const char **properties,
    void **res
    )
{
    CMPIInstance *inst = (CMPIInstance *)res_id;
    xen_utils_session *session = NULL;
    CMPIData data;
    static CMPIrc rc = CMPI_RC_OK;
    char *res_uuid=NULL;
    CMPIStatus status = {CMPI_RC_OK, NULL};
    (void)properties;

    CMPIObjectPath *op = CMGetObjectPath(inst, &status);
    CMPIString *cn = CMGetClassName(op, &status);
    if(CMIsNullObject(inst) || res == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error get(): inst or res is NULL"));
        return CMPI_RC_ERR_FAILED;
    }

    const char *key_prop = ft->xen_resource_get_key_property(broker, 
                                                             CMGetCharPtr(cn));
    data = CMGetProperty(inst, key_prop ,&status); 
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(data)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                     ("Error key_property %s couldnt be found for class %s", 
                      key_prop, CMGetCharPtr(cn)));
        return CMPI_RC_ERR_INVALID_PARAMETER;
    }

    /* Extract the resource identifier string from the CMPIString. */
    res_uuid = CMGetCharPtr(data.value.string);
    if(res_uuid == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Unable to extrace resource identifier string"));
        return CMPI_RC_ERR_FAILED;
    }

    if(!xen_utils_validate_session(&session, caller_id)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }

    provider_resource *prov_res = calloc(1, sizeof(provider_resource));
    if(prov_res == NULL) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Out of memory"));
        return CMPI_RC_ERR_FAILED;
    }

    prov_res->broker = broker;
    prov_res->classname = CMGetCharPtr(cn);
    prov_res->session = session;
    prov_res->cleanupsession = true;

    rc = ft->xen_resource_record_get_from_id(res_uuid, session, prov_res);
    if(rc != CMPI_RC_OK)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Error get(): get_xen_resource_record_from_id failed"));
        ft->xen_resource_record_cleanup(prov_res);
	xen_utils_cleanup_session(session);
        free(prov_res);
        return rc;
    }
    *res = (void *)prov_res;
    rc = CMPI_RC_OK;

    return rc;
}
/*****************************************************************************
 * Set CIM instance properties based on backend xen resource data
 *
 * @param in ft - xen backend provider function table
 * @param in/out inst - CIM instance whose properties are to be set
 * @param in res - xen resource
 * @param in properties - CIM properties that caller cares about
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_setproperties(
    const XenProviderInstanceFT* ft,
    CMPIInstance *inst, 
    const void *res,
    const char **properties)
{
    provider_resource *resource = (provider_resource *) res;
    if(res == NULL || CMIsNullObject(inst)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Invalid input parameter"));
        return CMPI_RC_ERR_FAILED;
    }

    RESET_XEN_ERROR(resource->session->xen);
    /* Setup a filter to only return the desired properties. */

    const char **keys = ft->xen_resource_get_keys(resource->broker, resource->classname);
    CMSetPropertyFilter(inst, properties, keys);
    return ft->xen_resource_set_properties(resource, inst);
}
/*****************************************************************************
 * Release a xen resource record
 *
 * @param in ft - xen backend provider function table
 * @param in res - provider specific data being released
 * @return CMPIrc error codes
 *****************************************************************************/
void prov_pxy_release(
    const XenProviderInstanceFT* ft,
    void *res)
{
    provider_resource *prov_res = (provider_resource *)res;
    if(prov_res)  {
        ft->xen_resource_record_cleanup(prov_res);
        if(prov_res->cleanupsession)
            xen_utils_cleanup_session(prov_res->session);
        if(prov_res)
            free(prov_res);
    }
}
/*****************************************************************************
 * Creates a new backend xen resource identified by the the CIM instance
 * This is not in use none of Xen's providers allow creation of a xen object
 * via the object's CreateInstance method. Instead specific 'extrinsic' methjods
 * are provided in a 'service' factory object.
 *
 * @param in broker - CMPI factory service broker
 * @param in ft - xen backend provider function table
 * @param in res_id - opaque data identifying the CIM object being added
 * @param in caller_id - CIM caller's credentials
 * @param in res - newly added resource
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_add(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id, 
    struct xen_call_context *caller_id, 
    const void *res
    )
{
    (void)res_id;
    (void)res;

    xen_utils_session *session = NULL;
    if(!xen_utils_validate_session(&session, caller_id)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Unable to establish connection with Xen"));  
        return CMPI_RC_ERR_FAILED;                             
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Add an instance"));
    CMPIrc rc = ft->xen_resource_add(broker, session, res_id);

    xen_utils_cleanup_session(session);
    return rc;
}
/*****************************************************************************
 * Delete a backend xen resource identified by the the CIM instance
 *
 * @param in broker - CMPI factory service broker
 * @param in ft - xen backend provider function table
 * @param in res_id - opaque data identifying the CIM object being deleted
 * @param in caller_id - CIM caller's credentials
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_delete(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id, 
    struct xen_call_context *caller_id
    )
{
    CMPIInstance *inst = (CMPIInstance *)res_id;
    CMPIData data;
    CMPIStatus status = {CMPI_RC_OK, NULL};

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Delete an instance"));
    CMPIObjectPath *op = CMGetObjectPath(inst, &status);
    CMPIString *cn = CMGetClassName(op, &status);

    xen_utils_session *session = NULL;   
    if(CMIsNullObject(inst)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Invalid parameter"));
        return CMPI_RC_ERR_FAILED;
    }

    const char *key_prop = ft->xen_resource_get_key_property(broker, CMGetCharPtr(cn));
    data = CMGetProperty(inst, key_prop ,&status); 
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(data)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get property"));
        return CMPI_RC_ERR_FAILED;
    }

    char *inst_id = CMGetCharPtr(data.value.string);       
    if((inst_id == NULL) || (*inst_id == '\0')) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get inst id"));
        return CMPI_RC_ERR_FAILED;
    }

    if(!xen_utils_validate_session(&session, caller_id)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("Unable to establish connection with Xen"));  
        return CMPI_RC_ERR_FAILED;                             
    }
    /* get the object and delete it */
    status.rc = ft->xen_resource_delete(broker, session, inst_id);

    xen_utils_cleanup_session(session);
    return status.rc;
}
/*****************************************************************************
 * Modify a backend xen resource identified by the the CIM instance based
 * on the instance's properties
 *
 * @param in broker - CMPI factory service broker
 * @param in ft - xen backend provider function table
 * @param in res_id - opaque data identifying the CIM object being modified
 * @param in caller_id - CIM caller's credentials
 * @param in modified_res - modifed version of the xen resource
 * @param in properties - CIM properties that caller cares about
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_modify(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const void *res_id,
    struct xen_call_context *caller_id, 
    const void *modified_res,
    const char **properties)
{
    (void)properties;
    CMPIData data;
    CMPIStatus status = {CMPI_RC_OK, NULL};
    char *inst_id;
    xen_utils_session *session = NULL;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO,("Modify an Instance"));
    CMPIInstance *inst = (CMPIInstance *)res_id;
    if(CMIsNullObject(inst))  {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("input parameter res_id is invalid"));
        return CMPI_RC_ERR_FAILED;
    }
    CMPIObjectPath *op = CMGetObjectPath(inst, &status);
    CMPIString *cn = CMGetClassName(op, &status);

    /* Get target resource */
    const char *key_prop = ft->xen_resource_get_key_property(broker, CMGetCharPtr(cn));
    data = CMGetProperty(inst, key_prop, &status);
    if((status.rc != CMPI_RC_OK) || CMIsNullValue(data)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get target resource"));
        return CMPI_RC_ERR_FAILED;
    }
    inst_id = CMGetCharPtr(data.value.string);
    if((inst_id == NULL) || (*inst_id == '\0')) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Could not get inst id"));
        return CMPI_RC_ERR_FAILED;
    }
    if(!xen_utils_validate_session(&session, caller_id)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("--- Unable to establish connection with Xen"));
        return CMPI_RC_ERR_FAILED;
    }

    /* Call the target provider */
    status.rc = ft->xen_resource_modify(broker, res_id, modified_res, properties, status, inst_id, session);

    xen_utils_cleanup_session(session);
    return status.rc;
}
/*****************************************************************************
 * Get the xen resource identified by the CIM instance properties
 *
 * @param in broker - broker for CMPI factory services
 * @param in ft - xen backend provider function table
 * @param in inst - CIM instance
 * @param in properties - CIM properties that caller cares about
 * @param out res - xen resource
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_extract(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const CMPIInstance *inst,
    const char **properties,
    void **res
    )
{
    /* Provider specific implementation may be done in the extract_resource function */
    return  ft->xen_resource_extract(res, inst, properties); 
}
/*****************************************************************************
 * Get resource id based on CMPI instance properties
 *
 * @param in broker - broker for CMPI factory services
 * @param in ft - xen backend provider function table
 * @param in inst - CIM instance
 * @param out res_id - xen resource identifier
 * @return CMPIrc error codes
 *****************************************************************************/
CMPIrc prov_pxy_extractid(
    const CMPIBroker *broker,
    const XenProviderInstanceFT* ft,
    const CMPIInstance* inst,
    void **res_id
    )
{
    *res_id = (void *)inst;
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Release the resource id 
 *
 * @param in ft - xen backend provider function table
 * @param in res_id - xen resource identifier to be released
 * @return CMPIrc error codes
 *****************************************************************************/
void prov_pxy_releaseid(
    const XenProviderInstanceFT* ft,
    void* res_id
    )
{
    (void)res_id;
}

/*****************************************************************************
 * Load a bakcned xen method provider based on the CIM Classname and 
 * initialize its function table
 *
 * @param in broker - CMPI services factory broker
 * @param in classname - CIM classname identifying the xen object
 * @return CMPIrc error codes
 *****************************************************************************/
const XenProviderMethodFT* prov_pxy_load_xen_method_provider(
    const CMPIBroker *broker,
    const char *classname
    )
{
    const XenProviderMethodFT* ft = NULL;
    int i;
    for (i=0; i<sizeof(g_method_providers)/sizeof(g_method_providers[0]); i++) {
        if(strcmp(g_method_providers[i].classname, classname) == 0) {
            ft = g_method_providers[i].provider_load_function();
        }
    }
    return ft;
}
