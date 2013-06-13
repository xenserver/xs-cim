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

#include <inttypes.h>

#include "providerinterface.h"

static const char *vs_cn = "Xen_VirtualSwitch";
static const char *vs_keys[] = {"CreationClassName","Name"}; 
static const char *vs_key_property = "Name";

static const char *vssd_cn = "Xen_VirtualSwitchSettingData";
static const char *vssd_keys[] = {"InstanceID"}; 
static const char *vssd_key_property = "InstanceID";

static const char * ncpool_cn = "Xen_NetworkConnectionPool";
static const char * ac_cn = "Xen_NetworkConnectionAllocationCapabilities";

static const char *pool_keys[] = {"InstanceID"}; 
static const char *pool_key_property = "InstanceID";

/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, vs_cn, classname))
         return vs_key_property;
    else if(xen_utils_class_is_subclass_of(broker, vssd_cn, classname))
        return vssd_key_property;
    else
        return pool_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if(xen_utils_class_is_subclass_of(broker, vs_cn, classname))
        return vs_keys;
    else if(xen_utils_class_is_subclass_of(broker, vssd_cn, classname))
        return vssd_keys;
    else
        return pool_keys;
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
    xen_network_set *network_set = NULL;
    if (!xen_network_get_all(session->xen, &network_set))
        return CMPI_RC_ERR_FAILED;
    resources->ctx = network_set;
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
    if(resources->ctx)
        xen_network_set_free((xen_network_set *)resources->ctx);
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
    xen_network_set *network_set = (xen_network_set *)resources_list->ctx;
    if (network_set == NULL || resources_list->current_resource == network_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_network_record *network_rec = NULL;
    if (!xen_network_get_record(session->xen, 
                                &network_rec, 
                                network_set->contents[resources_list->current_resource]
    ))
    {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    prov_res->ctx = network_rec;
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
    if(prov_res->ctx)
        xen_network_record_free((xen_network_record *)prov_res->ctx);
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
    //_CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf));
    if(xen_utils_class_is_subclass_of(prov_res->broker, vs_cn, prov_res->classname)) {
        /* Key is of the form 'UUID' */
        strncpy(buf, res_uuid, sizeof(buf)-1);
    }
    else if(xen_utils_class_is_subclass_of(prov_res->broker, ncpool_cn, prov_res->classname)) {
        _CMPIStrncpyDeviceNameFromID(buf, res_uuid, sizeof(buf)-1);
    }
    else {
        /* Key is of the form 'Xen:UUID'*/
        _CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf)-1);
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("xen rsource record get from id %s, %s", 
                                           prov_res->classname, buf))
    xen_network network;
    xen_network_record *network_rec = NULL;
    if(!xen_network_get_by_uuid(session->xen, &network, buf) || 
       !xen_network_get_record(session->xen, &network_rec, network))
    {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    prov_res->ctx = network_rec;
    xen_network_free(network);
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
static CMPIrc virtual_switch_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst)
{
    xen_network_record *network_rec  = (xen_network_record *)resource->ctx;

    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AvailableRequestedStates",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual Ethernet Switch", CMPI_chars);
    DMTF_CommunicationStatus comm_status = 
        DMTF_CommunicationStatus_Communication_OK;
    CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&comm_status, CMPI_uint16);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_VirtualSwitch", CMPI_chars);
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    DMTF_Dedicated dedicated = DMTF_Dedicated_Switch;
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&dedicated, CMPI_uint16);
    CMSetProperty(inst, "Dedicated",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Description",(CMPIValue *)network_rec->name_description, CMPI_chars);
    //CMSetProperty(inst, "DetailedStatus",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "ElementName",(CMPIValue *)network_rec->name_label, CMPI_chars);
    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    DMTF_EnabledState enabled_state = DMTF_EnabledState_Enabled;
    CMSetProperty(inst, "EnabledState",(CMPIValue *)&enabled_state, CMPI_uint16);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    DMTF_HealthState health_state = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&health_state, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, &<time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    char buf[MAX_INSTANCEID_LEN];
    _CMPICreateNewSystemInstanceID(buf, MAX_INSTANCEID_LEN, network_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)network_rec->uuid, CMPI_chars);
    //CMSetProperty(inst, "NameFormat",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OperatingStatus",(CMPIValue *)&opl_status, CMPI_uint16);
    arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    DMTF_OperationalStatus op_status = DMTF_OperationalStatus_OK;
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&op_status, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherDedicatedDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "PrimaryOwnerContact",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryOwnerName",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PrimaryStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "RequestedState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ResetCapability",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Roles",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "Status",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, &<time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TransitioningToState",(CMPIValue *)&<value>, CMPI_uint16);
    return CMPI_RC_OK;
}

static CMPIrc virtual_switch_setting_data_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst)
{
    bool shared_network = false; /* this is a pool wide network */
    xen_network_record *network_rec  = (xen_network_record *)resource->ctx;
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)network_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "AssociatedResourcePool",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "AutomaticRecoveryAction",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AutomaticShutdownAction",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AutomaticStartupAction",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, &<time_value>);
    //CMSetProperty(inst, "AutomaticStartupActionDelay",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "AutomaticStartupActionSequenceNumber",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual Ethernet Switch Settings", CMPI_chars);
    //CMSetProperty(inst, "ChangeableType",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ConfigurationDataRoot",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ConfigurationFile",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ConfigurationID",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ConfigurationName",(CMPIValue *)network_rec->bridge, CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, &<time_value>);
    //CMSetProperty(inst, "CreationTime",(CMPIValue *)&date_time, CMPI_dateTime);
    CMSetProperty(inst, "Description",(CMPIValue *)network_rec->name_description, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)network_rec->name_label, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    char buf[MAX_INSTANCEID_LEN];
    _CMPICreateNewSystemInstanceID(buf, MAX_INSTANCEID_LEN, network_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "LogDataRoot",(CMPIValue *)<value>, CMPI_chars);
    //int max_macs = 0xFFFF;
    //CMSetProperty(inst, "MaxNumMACAddress",(CMPIValue *)&<value>, CMPI_uint32);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Notes",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "RecoveryFile",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "SnapshotDataRoot",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "SuspendDataRoot",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "SwapFileDataRoot",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "VirtualSystemIdentifier",(CMPIValue *)network_rec->uuid, CMPI_chars);
    CMSetProperty(inst, "VirtualSystemType",(CMPIValue *)"DMTF:Virtual Ethernet Switch", CMPI_chars);

    if(network_rec->pifs) {
        xen_pif_record_opt_set *pif_opt_set = network_rec->pifs;
        if(pif_opt_set->size > 0) {
            CMPIArray *vlan = NULL;
            xen_pif_record_opt *pif_opt = network_rec->pifs->contents[0];
            int  num_hosts = 0;
            xen_host_set *host_set = NULL;
            if(xen_host_get_all(resource->session->xen, &host_set)) {
                num_hosts = host_set->size;
                xen_host_set_free(host_set);
            }
            if(num_hosts == pif_opt_set->size)
                shared_network = true; /* this network has a PIF on each host */
            if(pif_opt->is_record) {
                CMSetProperty(inst, "HostInterface",(CMPIValue *)pif_opt->u.record->device, CMPI_chars);
                if(pif_opt->u.record->vlan != -1) {
                    vlan = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
                    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%" PRId64, pif_opt->u.record->vlan);
                }
            } else {
                xen_pif_record *pif_rec = NULL;
                if(xen_pif_get_record(resource->session->xen, &pif_rec, pif_opt->u.handle) && pif_rec) {
                    CMSetProperty(inst, "HostInterface",(CMPIValue *)pif_rec->device, CMPI_chars);
                    if(pif_rec->vlan != -1) {
                        vlan = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
                        snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%" PRId64, pif_rec->vlan);
                    }
                    xen_pif_record_free(pif_rec);
                }
            }
            if(vlan) {
                CMSetArrayElementAt(vlan, 0, (CMPIValue *) buf, CMPI_chars);
                CMSetProperty(inst, "VLANConnection",(CMPIValue *)&vlan, CMPI_charsA);
            }
        }
    }
    CMSetProperty(inst, "Shared",(CMPIValue *)&shared_network, CMPI_boolean);
    CMSetProperty(inst, "Bridge",(CMPIValue *)network_rec->bridge, CMPI_chars);
    if(network_rec->other_config && network_rec->other_config->size > 0) {
        arr = xen_utils_convert_string_string_map_to_CMPIArray(resource->broker, network_rec->other_config);
        CMSetProperty(inst, "OtherConfig",(CMPIValue *)&arr, CMPI_charsA);
    }

    // 
    return CMPI_RC_OK;
}

static CMPIrc pool_instance_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    CMPIInstance *inst)
{
    xen_host_record *host_rec = NULL;
    char *host_uuid = "NoHost";
    xen_pif_record *pif_rec = NULL;
    bool b_recs_to_be_freed = false;
    int rc = 0;
    xen_pif_record_opt_set *pif_opt_set = NULL;
    xen_pif_record_opt* pif_opt = NULL;
    xen_host_record_opt* host_opt = NULL;

    xen_network_record *network_rec  = (xen_network_record *)resource->ctx;
    pif_opt_set = network_rec->pifs;
    if(pif_opt_set && pif_opt_set->size > 0)
    {
        xen_host host = NULL;
        pif_opt = pif_opt_set->contents[0];
        if(!pif_opt->is_record)
        {
            xen_pif_get_record(resource->session->xen, &pif_rec, pif_opt->u.handle);
            xen_pif_get_host(resource->session->xen, &host, pif_opt->u.handle);
            if(host)
            {
                xen_host_get_record(resource->session->xen, &host_rec, host);
                b_recs_to_be_freed = true;
                host_uuid = host_rec->uuid;
            }
            xen_host_free(host);
        }
        else
        {
            pif_rec = pif_opt->u.record;
            host_opt = pif_opt->u.record->host;
            if(host_opt)
            {
                if(host_opt->is_record)
                    host_rec = host_opt->u.record;
                else
                {
                    xen_host_get_record(resource->session->xen, &host_rec, host_opt->u.handle);
                    b_recs_to_be_freed = true;
                    host_uuid = host_rec->uuid;
                }
            }
        }
    }
    else
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_WARNING,
            ("--- No PIF info and hence no Host info"));
    }
    char buf[MAX_INSTANCEID_LEN];

    /* Key properties */
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, host_uuid, network_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    /* Populate the instance's properties with the backend data */
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"count", CMPI_chars);
    uint64_t capacity = 0xFFFFFFFF; /* unlimited network ports/connections available */
    CMSetProperty(inst, "Capacity",(CMPIValue *)&capacity, CMPI_uint64);

#if XENAPI_VERSION > 400
    snprintf(buf, sizeof(buf)/sizeof(buf[0])-1,
        "Device=%s,Bridge=%s",
        (pif_rec ? pif_rec->device:"No_Device_info"), network_rec->bridge);
#endif
    CMSetProperty(inst, "Description",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)(host_rec ? host_rec->name_label : "Host info Unavailable"), CMPI_chars);
    DMTF_HealthState hState = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState",(CMPIValue *)&hState, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(resource->broker, &<time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "MaxConsumableResource",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "ConsumedResourceUnits",(CMPIValue *)"count", CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)network_rec->name_label, CMPI_chars);
    DMTF_OperationalStatus opStatus = DMTF_OperationalStatus_OK;
    CMPIArray *arr = CMNewArray(broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)&opStatus, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "PoolID",(CMPIValue *)network_rec->uuid, CMPI_chars);
    bool primordial = true;
    CMSetProperty(inst, "Primordial",(CMPIValue *)&primordial, CMPI_boolean);
    uint64_t reserved = 0;
    CMSetProperty(inst, "Reserved",(CMPIValue *)&reserved, CMPI_uint64);
    DMTF_ResourceType resType;
    char *caption = NULL;
    if(strcmp(resource->classname, ncpool_cn)==0) {
        caption = "Network connection resources available to establish a connection between a virtual switch and a virtual system (VM).";
        resType = DMTF_ResourceType_Ethernet_Connection;
    }
    CMSetProperty(inst, "Caption",(CMPIValue *) caption, CMPI_chars);
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&resType, CMPI_uint16);
    CMSetProperty(inst, "Status",(CMPIValue *)DMTF_Status_OK, CMPI_chars);
    //CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    rc = CMPI_RC_OK;

    if(b_recs_to_be_freed)
    {
        xen_host_record_free(host_rec);
        xen_pif_record_free(pif_rec);
    }
    return rc;
}

static CMPIrc allocation_capabilities_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    CMPIInstance *inst)
{
    char buf[MAX_INSTANCEID_LEN];
    xen_network_record *network_rec  = (xen_network_record *)resource->ctx;

    CMSetProperty(inst, "Caption",(CMPIValue *)"Allocation Capabilities of a Xen network connection", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)"Allocation Capabilities of a Xen network connection", CMPI_chars);
    CMSetProperty(inst, "ElementName",(CMPIValue *)network_rec->name_label, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf)/sizeof(buf[0])-1, 
                                   network_rec->uuid,  "NetworkConnectionAllocationCapabilities");
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "RequestTypesSupported",(CMPIValue *)&<value>, CMPI_uint16);
    int res_type = DMTF_ResourceType_Ethernet_Connection;
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)&res_type, CMPI_chars);
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

static CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst)
{
    if(xen_utils_class_is_subclass_of(resource->broker, vs_cn, resource->classname))
        return virtual_switch_set_properties(resource, inst);
    else if(xen_utils_class_is_subclass_of(resource->broker, vssd_cn, resource->classname))
        return virtual_switch_setting_data_set_properties(resource, inst);
    else if(xen_utils_class_is_subclass_of(resource->broker, ac_cn, resource->classname))
        return allocation_capabilities_set_properties(resource->broker, resource, inst);
    else
        return pool_instance_set_properties(resource->broker, resource, inst);

}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_VirtualSwitch)

/******************************************************************************
 Helper functions for other providers to use
******************************************************************************/
/******************************************************************************
* vssd_to_net_rec
*
* Parses a Xen_VirtualSwitchSettingData CIM instance and populates a
* xen network record.
*
* Returns 1 on Success and 0 on failure.
******************************************************************************/
int vssd_to_network_rec(
    const CMPIBroker* broker,
    CMPIInstance *vssd,
    xen_network_record** net_rec_out,
    CMPIStatus *status
    )
{
    CMPIData propertyvalue;
    CMPIObjectPath* objectpath = NULL;
    xen_network_record *net_rec = NULL;
    int statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Invalid Parameter 'SystemSettings'";

    _SBLIM_ENTER("vssd_to_net_rec");

    /* Get the class type of the setting data instance */
    objectpath = CMGetObjectPath(vssd, NULL);
    char *vssdcn = CMGetCharPtr(CMGetClassName(objectpath, NULL));
    if(!xen_utils_class_is_subclass_of(broker, vssdcn, "CIM_VirtualSystemSettingData")){
        error_msg = "ERROR: SystemSettings passed in is not CIM_VirtualSystemSettingData or Xen_VirtualSwitchSettingData";
        goto Exit;
    }

    net_rec = xen_network_record_alloc();
    if(net_rec == NULL)
        goto Exit;

    propertyvalue = CMGetProperty(vssd, "InstanceID", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
       if(propertyvalue.type == CMPI_string) {
            char buf[MAX_INSTANCEID_LEN];
            _CMPIStrncpySystemNameFromID(buf, CMGetCharPtr(propertyvalue.value.string), sizeof(buf)/sizeof(buf[0]));
            net_rec->uuid = strdup(buf);
       }
       else
           goto Exit;
    }

    propertyvalue = CMGetProperty(vssd, "ElementName", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        if(propertyvalue.type == CMPI_string)
            net_rec->name_label = strdup(CMGetCharPtr(propertyvalue.value.string));
        else
            goto Exit;
    }

    propertyvalue = CMGetProperty(vssd, "Description", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        if(propertyvalue.type == CMPI_string)
            net_rec->name_description = strdup(CMGetCharPtr(propertyvalue.value.string));
        else
            goto Exit;
    }
    propertyvalue = CMGetProperty(vssd, "OtherConfig", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue)) {
        if(propertyvalue.type == CMPI_stringA)
            net_rec->other_config = xen_utils_convert_CMPIArray_to_string_string_map(propertyvalue.value.array);
    }
    if(net_rec->other_config == NULL)
        net_rec->other_config = xen_string_string_map_alloc(0);

    *net_rec_out = net_rec;
    statusrc = CMPI_RC_OK;

Exit:
     xen_utils_set_status(broker, status, statusrc, error_msg, NULL);
    return (statusrc == CMPI_RC_OK);
}

CMPIObjectPath* virtual_switch_create_ref(
    const CMPIBroker *broker,
    xen_utils_session *session,
    xen_network network,
    CMPIStatus *status
    )
{
    CMPIObjectPath *op = NULL;
    char *uuid;
    if(xen_network_get_uuid(session->xen, &uuid, network))
    {
        op = CMNewObjectPath(broker, DEFAULT_NS, "Xen_VirtualSwitch", status);
        CMAddKey(op, "Name", (CMPIValue *)uuid, CMPI_chars);
        CMAddKey(op, "CreationClassName", (CMPIValue *)"Xen_VirtualSwitch", CMPI_chars);
        free(uuid);
    }
    return op;
}

