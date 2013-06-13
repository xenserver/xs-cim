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

#include "providerinterface.h"
#include "RASDs.h"

typedef struct {
    xen_vif vif;
    xen_vif_record * vif_rec;
}local_vif_resource;

static const char *np_cn = "Xen_NetworkPort";
static const char *vsp_cn = "Xen_VirtualSwitchPort";
static const char *np_keys[] = {"SystemName","CreationClassName","SystemCreationClassName","DeviceID"}; 
static const char *np_key_property = "DeviceID"; 

static const char *lecp_cn = "Xen_ComputerSystemLANEndpoint";
static const char *levs_cn = "Xen_VirtualSwitchLANEndpoint";
static const char *le_keys[] = {"SystemName","CreationClassName","SystemCreationClassName","Name"}; 
static const char *le_key_property = "Name"; 

static const char *nmr_cn = "Xen_NetworkPortReceiveThroughput";
static const char *nmt_cn = "Xen_NetworkPortTransmitThroughput";
static const char *nm_keys[] = {"InstanceID"}; 
static const char *nm_key_property = "InstanceID";

//static const char *rasd_cn = "Xen_NetworkPortSettingData";         
static const char *rasd_keys[] = {"InstanceID"}; 
static const char *rasd_key_property = "InstanceID";

/* Forward declarations */
static void _set_network_port_properties(
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    );
static void _set_lanendpoint_properties(
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    );
static void _set_network_metrics_properties(
    const CMPIBroker *broker,
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    );
static void _set_network_port_rasd_properties(
    const CMPIBroker *broker,
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    );
static char *_get_ip_address(
    xen_vif_record *vif_rec,
    xen_vm_guest_metrics_record *metrics_rec
    );
/********************************************************
 * Provider export functions 
 *******************************************************/
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, np_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, vsp_cn, classname))
        return np_key_property;
    else if (xen_utils_class_is_subclass_of(broker, lecp_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, levs_cn, classname))
        return le_key_property;
    else if (xen_utils_class_is_subclass_of(broker, nmr_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, nmt_cn, classname))
        return nm_key_property;
    else
        return rasd_key_property;
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, np_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, vsp_cn, classname))
        return np_keys;
    else if (xen_utils_class_is_subclass_of(broker, lecp_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, levs_cn, classname))
        return le_keys;
    else if (xen_utils_class_is_subclass_of(broker, nmr_cn, classname) ||
        xen_utils_class_is_subclass_of(broker, nmt_cn, classname))
        return nm_keys;
    else
        return rasd_keys;
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
    provider_resource_list *resources)
{
    xen_vif_set *vif_set = NULL;
    if (!xen_vif_get_all(session->xen, &vif_set)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        free(resources);
        return CMPI_RC_ERR_FAILED;
    }

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Enumerated %d network ports", vif_set->size));
    resources->ctx = vif_set;
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
        xen_vif_set_free((xen_vif_set *)resources->ctx);
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
    xen_vif_record *vif_rec = NULL;
    xen_vif_set *vif_set = (xen_vif_set *)resources_list->ctx;
    if (vif_set == NULL || resources_list->current_resource == vif_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    if (!xen_vif_get_record(resources_list->session->xen, 
        &vif_rec, 
        vif_set->contents[resources_list->current_resource])) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Getting next network port"));
    local_vif_resource *ctx = calloc(sizeof(local_vif_resource), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->vif = vif_set->contents[resources_list->current_resource];
    ctx->vif_rec = vif_rec;
    vif_set->contents[resources_list->current_resource]  = NULL;
    prov_res->ctx = ctx;
    return CMPI_RC_OK;
}
/*****************************************************************************
 * Function to cleanup the resource
 *
 * @param - provider_resource to be freed
 * @return CMPIrc error codes
****************************************************************************/
static CMPIrc xen_resource_record_cleanup(provider_resource *prov_res)
{
    local_vif_resource *ctx = prov_res->ctx;
    if (ctx) {
        if (ctx->vif_rec)
            xen_vif_record_free(ctx->vif_rec);
        if (ctx->vif)
            xen_vif_free(ctx->vif);
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
    xen_vif vif;
    xen_vif_record *vif_rec = NULL;

    char buf[MAX_INSTANCEID_LEN];
    if(xen_utils_class_is_subclass_of(
        prov_res->broker, prov_res->classname, 
        "CIM_LANEndpoint")) {
        /* key property is in UUID form */
        strncpy(buf, res_id, sizeof(buf));
    } else {
        /* key proeprty is in Xen:VMUUID/DevUUID form */
        _CMPIStrncpyDeviceNameFromID(buf, res_id, sizeof(buf));
    }
    if (!xen_vif_get_by_uuid(session->xen, &vif, buf)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    if (!xen_vif_get_record(session->xen, &vif_rec, vif)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    local_vif_resource *ctx = calloc(sizeof(local_vif_resource), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->vif_rec = vif_rec;
    ctx->vif = vif;
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
    xen_vif_record *vif_rec = ((local_vif_resource *)resource->ctx)->vif_rec;
    if (vif_rec == NULL || CMIsNullObject(inst))
        return CMPI_RC_ERR_FAILED;

    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("set properties for %s", resource->classname));
    if (xen_utils_class_is_subclass_of(resource->broker, np_cn, resource->classname) ||
        xen_utils_class_is_subclass_of(resource->broker, vsp_cn, resource->classname))
        _set_network_port_properties(resource, vif_rec, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, lecp_cn, resource->classname) ||
        xen_utils_class_is_subclass_of(resource->broker, levs_cn, resource->classname))
        _set_lanendpoint_properties(resource, vif_rec,  inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, nmr_cn, resource->classname) ||
        xen_utils_class_is_subclass_of(resource->broker, nmt_cn, resource->classname))
        _set_network_metrics_properties(resource->broker, resource, vif_rec, inst);
    else
        _set_network_port_rasd_properties(resource->broker, resource, vif_rec, inst);

    return CMPI_RC_OK;
}

static char *_get_ip_address(
    xen_vif_record *vif_rec,
    xen_vm_guest_metrics_record *metrics_rec
    )
{
    char key[100];
    char *ip_address = NULL;
    if (vif_rec && metrics_rec) {
        snprintf(key, sizeof(key)/sizeof(key[0]), "%s/ip", vif_rec->device);
        ip_address = xen_utils_get_from_string_string_map(metrics_rec->networks, key);
    }
    return ip_address;
}
static void _set_network_port_properties(
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    )
{
    char *dom_uuid;
    xen_network_record *net_rec = NULL;
    xen_vif_metrics vif_metrics = NULL;
    xen_vif_metrics_record *vif_metrics_rec = NULL;
    xen_vm_guest_metrics metrics = NULL;
    xen_vm_guest_metrics_record *metrics_rec = NULL;

    uint64_t bandwidth = 0;
    char buf[MAX_INSTANCEID_LEN];

    if (!xen_vm_get_uuid(resource->session->xen, &dom_uuid, vif_rec->vm->u.handle)) {
        xen_utils_trace_error(resource->session->xen, __FILE__, __LINE__);
        return;
    }
    if (vif_rec->network->is_record)
        net_rec = vif_rec->network->u.record;
    else
        xen_network_get_record(resource->session->xen, &net_rec, vif_rec->network->u.handle);

    if (xen_vif_get_metrics(resource->session->xen, &vif_metrics, ((local_vif_resource *)resource->ctx)->vif)
        && vif_metrics)
        xen_vif_metrics_get_record(resource->session->xen, &vif_metrics_rec, vif_metrics);
    RESET_XEN_ERROR(resource->session->xen);

    if (xen_vm_get_guest_metrics(resource->session->xen, &metrics, vif_rec->vm->u.handle) && metrics)
        xen_vm_guest_metrics_get_record(resource->session->xen, &metrics_rec, metrics);
    RESET_XEN_ERROR(resource->session->xen);

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "AdditionalAvailablility", (CMPIValue *)"Automatic", CMPI_chars);
    bool autoSense = false;
    CMSetProperty(inst, "AutoSense", (CMPIValue *) &autoSense, CMPI_boolean);
    int availability = DMTF_Availability_Off_Line;
    if (vif_rec->currently_attached)
        availability = DMTF_Availability_Running_Full_Power;
    CMSetProperty(inst, "Availability", (CMPIValue *) &availability, CMPI_uint16);
    CMPIArray *cap_arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    int cap = 0x0; //Unknown, as opposed to AlertOnLan, WakeOnLan, Failover and LoadBalancing
    CMSetArrayElementAt(cap_arr, 0, (CMPIValue*)&cap, CMPI_uint16);
    CMSetProperty(inst, "Capabilities", (CMPIValue *) &cap_arr, CMPI_uint16A);
    CMSetProperty(inst, "EnabledCapabilities", (CMPIValue *) &cap_arr, CMPI_uint16A);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)resource->classname, CMPI_chars);
    if (net_rec) {
        CMSetProperty(inst, "Caption",(CMPIValue *)net_rec->name_label, CMPI_chars);
        CMSetProperty(inst, "Description",(CMPIValue *)net_rec->name_description, CMPI_chars);
        CMSetProperty(inst, "ElementName", (CMPIValue *)net_rec->name_description, CMPI_chars);
    }

    char *ip_address = _get_ip_address(vif_rec, metrics_rec);
    if (ip_address && (*ip_address != '\0') ) {
        CMPIArray *ar = CMNewArray(resource->broker, 1, CMPI_string, NULL);
        CMPIString *val = CMNewString(resource->broker, ip_address, NULL);
        CMSetArrayElementAt(ar, 0, (CMPIValue*)&val, CMPI_string);
        CMSetProperty(inst, "NetworkAddresses", (CMPIValue *)&ar, CMPI_stringA); // charsA causes a crash.
    }
    int enabled_default = DMTF_EnabledDefault_Enabled;
    CMSetProperty(inst, "EnabledDefault", (CMPIValue *)&enabled_default, CMPI_uint16);
    CMSetProperty(inst, "EnabledState", (CMPIValue *)&availability, CMPI_uint16);
    bool error_cleared = true;
    if (vif_rec->status_code)
        error_cleared = false;
    CMSetProperty(inst, "ErrorCleared", (CMPIValue *)&error_cleared, CMPI_boolean);
    CMSetProperty(inst, "ErrorDescription", (CMPIValue *)vif_rec->status_detail, CMPI_chars);
    bool full_duplex = true;
    CMSetProperty(inst, "FullDuplex", (CMPIValue *) &full_duplex, CMPI_boolean);
    int healthState = DMTF_HealthState_OK;
    CMSetProperty(inst, "HealthState", (CMPIValue *)&healthState, CMPI_uint16);
    CMSetProperty(inst, "IsBound", (CMPIValue *)&vif_rec->currently_attached, CMPI_boolean);
    CMSetProperty(inst, "LastErrorCode", (CMPIValue *)&vif_rec->status_code, CMPI_uint32);
    int link_tech = DMTF_LinkTechnology_Ethernet;
    CMSetProperty(inst, "LinkTechnology",(CMPIValue *)&link_tech, CMPI_uint16);

    if (vif_metrics_rec)
        bandwidth = vif_metrics_rec->io_read_kbs * 1024 * 8; /* in bits per sec */
    CMSetProperty(inst, "MaxSpeed", (CMPIValue *) &bandwidth, CMPI_uint64);
    CMSetProperty(inst, "Name",(CMPIValue *)vif_rec->uuid, CMPI_chars);

    /* Stuff the IP addresses assigned to this VIF into the Network Address field */
    int op_status = DMTF_OperationalStatus_OK;
    CMPIArray *op = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(op, 0, (CMPIValue *)&op_status, CMPI_uint16);
    CMSetProperty(inst, "OperationalStatus", (CMPIValue *)&op, CMPI_uint16A);
    if (vif_rec->mac)
        CMSetProperty(inst, "PermanentAddress",(CMPIValue *)vif_rec->mac, CMPI_chars);
    int port_type = DMTF_PortType_Unknown;
    CMSetProperty(inst, "PortType",(CMPIValue *)&port_type, CMPI_uint16);
    int power_management_cap = DMTF_PowerManagementCapabilities_Not_Supported;
    CMPIArray *power_mgmt_arr = CMNewArray(resource->broker, 1, CMPI_uint16, NULL);
    CMSetArrayElementAt(power_mgmt_arr, 0, (CMPIValue *)&power_management_cap, CMPI_uint16);
    CMSetProperty(inst, "PowerManagementCapabilities", (CMPIValue *) &power_mgmt_arr, CMPI_uint16A);
    bool supported = (power_management_cap != 1);
    CMSetProperty(inst, "PowerManagementSupported", (CMPIValue *)&supported, CMPI_boolean);
    int req_speed = 0;
    CMSetProperty(inst, "RequestedSpeed", (CMPIValue *) &req_speed, CMPI_uint32);
    int requested_state = DMTF_RequestedState_No_Change;
    CMSetProperty(inst, "RequestedState", (CMPIValue *) &requested_state, CMPI_uint16);
    CMSetProperty(inst, "Speed", (CMPIValue *) &bandwidth, CMPI_uint64);

    char *status = DMTF_Status_OK;
    if (vif_rec->currently_attached)
        status = DMTF_Status_No_Contact;
    CMSetProperty(inst, "Status", (CMPIValue *) status, CMPI_chars);
    int status_info = DMTF_StatusInfo_Enabled;
    CMSetProperty(inst, "StatusInfo", (CMPIValue *) &status_info, CMPI_uint16);

    if (strcmp(resource->classname, np_cn) == 0) {
        CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
        _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, dom_uuid, vif_rec->uuid);
        CMSetProperty(inst, "SystemName",(CMPIValue *)dom_uuid, CMPI_chars);
    }
    else if (strcmp(resource->classname, vsp_cn) == 0) {
        if (net_rec) {
            CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_VirtualSwitch", CMPI_chars);
            CMSetProperty(inst, "SystemName",(CMPIValue *)net_rec->uuid, CMPI_chars);
            _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, net_rec->uuid, vif_rec->uuid);
        }
    }
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);

    if (vif_rec->mtu > 0) {
        CMSetProperty(inst, "SupportedMaximumTransmissionUnit",(CMPIValue *)&(vif_rec->mtu), CMPI_uint64);
        CMSetProperty(inst, "ActiveMaximumTransmissionUnit",(CMPIValue *)&(vif_rec->mtu), CMPI_uint64);
        CMSetProperty(inst, "MaxDataSize", (CMPIValue *)&(vif_rec->mtu), CMPI_uint32);
    }
    int usage_restriction = DMTF_UsageRestriction_Not_restricted;
    CMSetProperty(inst, "UsageRestriction",  (CMPIValue *)&usage_restriction, CMPI_uint16);

    /* The following are Xen_networkPort class specific */
#if 0
    char nic_config_info[512];
    nic_config_info[0] = '\0';
    if (nic_config_info[0] != '\0')
        CMSetProperty(inst, "NICConfigInfo" , (CMPIValue *)nic_config_info, CMPI_chars);
#endif
    /* Not used */
    //CMSetProperty(inst, "CapabilityDescriptions", (CMPIValue *)descs, CMPI_stringA);
    //CMSetProperty(inst, "IdentifyingDescriptions", (CMPIValue *)"", CMPI_string);
    //CMSetProperty(inst, "LocationIndicator", (CMPIValue *)..., CMPI_uin16);
    // CMSetProperty(inst, "OtherEnabledCapabilities", (CMPIValue *)&cap, CMPI_stringA);
    // CMSetProperty(inst, "OtherEnabledState", (CMPIValue *)&state, CMPI_string);
    // CMSetProperty(inst, "OtherIdentifyingInfo", (CMPIValue *)&id_info, CMPI_string);
    // CMSetProperty(inst, "OtherLinkTechnology", (CMPIValue *)&other_link_tech, CMPI_string);
    // CMSetProperty(inst, "OtherNetworkPortType", (CMPIValue *)&other_np_type, CMPI_string);
    //int port_num = 0;
    //CMSetProperty(inst, "PortNumber", (CMPIValue *)&port_num, CMPI_uint16);
    //CMSetProperty(inst, "PowerOnHours", (CMPIValue *)&hrs, CMPI_uint64);
    //CMSetProperty(inst, "StatusDescriptions", (CMPIValue *) &status_desc_arr, CMPI_stringA);
    //CMSetProperty(inst, "TimeOfLastStateChange", (CMPIValue *) &time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours", (CMPIValue *) &power_on_hrs, CMPI_uint64);
    //CMSetProperty(inst, "NICConfigInfo",(CMPIValue *)resource->vif[vifnum].params, CMPI_chars);
    if (dom_uuid)
        free(dom_uuid);
    if (net_rec && !vif_rec->network->is_record)
        xen_network_record_free(net_rec);
    if (vif_metrics)
        xen_vif_metrics_free(vif_metrics);
    if (vif_metrics_rec)
        xen_vif_metrics_record_free(vif_metrics_rec);
    if (metrics)
        xen_vm_guest_metrics_free(metrics);
    if (metrics_rec)
        xen_vm_guest_metrics_record_free(metrics_rec);
}
static void _set_lanendpoint_properties(
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    )
{
    CMSetProperty(inst, "Caption",(CMPIValue *)"LAN Endpoint", CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *) resource->classname, CMPI_chars);
    CMSetProperty(inst, "MACAddress",(CMPIValue *)vif_rec->mac, CMPI_chars);
    CMSetProperty(inst, "Name",(CMPIValue *)vif_rec->uuid, CMPI_chars);
    if (strcmp(resource->classname, "Xen_ComputerSystemLANEndpoint") == 0) {
        char *dom_uuid = NULL;
        xen_vm_get_uuid(resource->session->xen, &dom_uuid, vif_rec->vm->u.handle);
        CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_ComputerSystem", CMPI_chars);
        if (dom_uuid) {
            CMSetProperty(inst, "SystemName",(CMPIValue *)dom_uuid, CMPI_chars);
            free(dom_uuid);
        }

    }
    else {
        xen_network_record *net_rec = NULL;
        if (vif_rec->network->is_record)
            net_rec = vif_rec->network->u.record;
        else
            xen_network_get_record(resource->session->xen, &net_rec, vif_rec->network->u.handle);
        CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_VirtualSwitch", CMPI_chars);
        CMSetProperty(inst, "SystemName",(CMPIValue *)net_rec->uuid, CMPI_chars);
        if (!vif_rec->network->is_record)
            xen_network_record_free(net_rec);

    }
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TransitioningToState",(CMPIValue *)&<value>, CMPI_uint16);

}
static void _set_network_metrics_properties(
    const CMPIBroker *broker,
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    )
{
    char buf[MAX_INSTANCEID_LEN];
    xen_vm_record *vm_rec = NULL;

    RESET_XEN_ERROR(resource->session->xen);
    if (xen_vm_get_record(resource->session->xen, &vm_rec, vif_rec->vm->u.handle) && vm_rec)
        _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec->uuid, vif_rec->uuid);
    else
        _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, "NoHost", vif_rec->uuid);

    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    // Is this the MetricsDefinitionID or the classname ?
    snprintf(buf, MAX_INSTANCEID_LEN, "%sDef", resource->classname);
    CMSetProperty(inst, "MetricDefinitionId",(CMPIValue *)buf, CMPI_chars);
    //CMSetProperty(inst, "BreakdownDimension",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "BreakdownValue",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Virtual Network Port metrics", CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "Duration",(CMPIValue *)&date_time, CMPI_dateTime);
    CMSetProperty(inst, "ElementName",(CMPIValue *)vif_rec->device, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    if (vm_rec)
        CMSetProperty(inst, "MeasuredElementName",(CMPIValue *)vm_rec->name_label, CMPI_chars);
    // 
    double io_kbps = 0.0;
    if (strcmp(resource->classname, nmr_cn) == 0)
        snprintf(buf, MAX_INSTANCEID_LEN, "vif_%s_rx", vif_rec->device);
    else
        snprintf(buf, MAX_INSTANCEID_LEN, "vif_%s_tx", vif_rec->device);
    CMSetProperty(inst, "Description",(CMPIValue *)buf, CMPI_chars);
    xen_vm_query_data_source(resource->session->xen, &io_kbps, vif_rec->vm->u.handle, buf);
    snprintf(buf, MAX_INSTANCEID_LEN, "%f", io_kbps);
    CMSetProperty(inst, "MetricValue", (CMPIValue *)buf, CMPI_chars);

    CMPIDateTime *date_time = xen_utils_CMPIDateTime_now(broker);
    CMSetProperty(inst, "TimeStamp",(CMPIValue *)&date_time, CMPI_dateTime);
    bool vol = true;
    CMSetProperty(inst, "Volatile",(CMPIValue *)&vol, CMPI_boolean);

    if (vm_rec)
        xen_vm_record_free(vm_rec);
}
static void _set_network_port_rasd_properties(
    const CMPIBroker *broker,
    provider_resource *resource,
    xen_vif_record *vif_rec,
    CMPIInstance *inst
    )
{  
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("NetworkPortSettingData for %s", vif_rec->uuid));
    network_rasd_from_vif(broker, resource->session, vif_rec, inst);
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_NetworkPort)
    
