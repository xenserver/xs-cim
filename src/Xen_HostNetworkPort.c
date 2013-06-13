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
#include <provider_common.h>
#include <RASDs.h>
#include <cmpitrace.h>
#include "providerinterface.h"
#include "RASDs.h"

static const char *hnp_cn = "Xen_HostNetworkPort";    
static const char *rasd_cn = "Xen_HostNetworkPortSettingData";    
static const char *rx_metrics_cn = "Xen_HostNetworkPortReceiveThroughput";
//static const char *tx_metrics_cn = "Xen_HostNetworkPortTransmitThroughput";

static const char *hnp_keys[] = {"SystemName","CreationClassName", "SystemCreationClassName","DeviceID"}; 
static const char *rasd_keys[] = {"InstanceID"};
static const char *metrics_keys[] = {"InstanceID"};

static const char *hnp_key_property = "DeviceID";
static const char *rasd_key_property = "InstanceID";
static const char *metrics_key_property = "InstanceID";

typedef struct {
    xen_pif pif;
    xen_pif_record *pif_rec;
} local_pif_resource;
/*********************************************************
 ************ Provider Specific functions **************** 
 ******************************************************* */
static const char *xen_resource_get_key_property(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, classname, hnp_cn)){
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("subclass of hostnetworkport"))
        return hnp_key_property;
    }
    else if (xen_utils_class_is_subclass_of(broker, classname, rasd_cn)){
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("subclass of hostnetworkportsettingdata"))
        return rasd_key_property;
    }
    else {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("subclass of metrics"))
        return metrics_key_property;
    }
}
static const char **xen_resource_get_keys(
    const CMPIBroker *broker,
    const char *classname
    )
{
    if (xen_utils_class_is_subclass_of(broker, classname, hnp_cn))
        return hnp_keys;
    else if (xen_utils_class_is_subclass_of(broker, classname, rasd_cn))
        return rasd_keys;
    else
        return metrics_keys;
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
    xen_pif_set *pif_set = NULL, *all_pifs = NULL;
    if (!xen_pif_get_all(session->xen, &all_pifs))
        return CMPI_RC_ERR_FAILED;

    /* Filter out physical NICS that are part of a BOND */
    /* SCVMM team needs this for some reason */
    int i = 0;
    if (all_pifs) {
        for (i=0; i<all_pifs->size; i++) {
            xen_bond bond = NULL;
            RESET_XEN_ERROR(session->xen);
            if (xen_pif_get_bond_slave_of(session->xen, &bond, all_pifs->contents[i]) && bond) {
                char *uuid = NULL;
                xen_bond_get_uuid(session->xen, &uuid, bond);
                xen_bond_free(bond);
                if(uuid) {
                    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("PIF is bond slave of %s", uuid));
                    free(uuid);
                    continue;
                }
            }
            /* Include only NICs that are not part of the bond */
            ADD_DEVICE_TO_LIST(pif_set, all_pifs->contents[i], xen_pif);
        }
        xen_pif_set_free(all_pifs);
    }
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR, ("Enumerated %d PIFs", pif_set->size));
    resources->ctx = pif_set;
    return CMPI_RC_OK;

    Exit:
    if (all_pifs)
        xen_pif_set_free(all_pifs);
    if (pif_set)
        xen_pif_set_free(pif_set);
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
    if (resources->ctx)
        xen_pif_set_free((xen_pif_set *)resources->ctx);
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
    xen_pif_set *pif_set = (xen_pif_set *)resources_list->ctx;
    if (pif_set == NULL || resources_list->current_resource == pif_set->size)
        return CMPI_RC_ERR_NOT_FOUND;

    xen_pif_record *pif_rec = NULL;
    if (!xen_pif_get_record(session->xen, 
                            &pif_rec, 
                            pif_set->contents[resources_list->current_resource]
        )) {
        xen_utils_trace_error(resources_list->session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_FAILED;
    }
    local_pif_resource *ctx = calloc(sizeof(local_pif_resource), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->pif = pif_set->contents[resources_list->current_resource];
    ctx->pif_rec = pif_rec;
    pif_set->contents[resources_list->current_resource] = NULL; /* do not delete this*/
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
    if (prov_res->ctx) {
        local_pif_resource *ctx = prov_res->ctx;
        if (ctx->pif_rec)
            xen_pif_record_free(ctx->pif_rec);
        if (ctx->pif)
            xen_pif_free(ctx->pif);
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
    _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("hostnetwork port %s", buf));
    //_CMPIStrncpySystemNameFromID(buf, res_uuid, sizeof(buf));
    xen_pif pif;
    xen_pif_record *pif_rec = NULL;
    if (!xen_pif_get_by_uuid(session->xen, &pif, buf) || 
        !xen_pif_get_record(session->xen, &pif_rec, pif)) {
        xen_utils_trace_error(session->xen, __FILE__, __LINE__);
        return CMPI_RC_ERR_NOT_FOUND;
    }
    local_pif_resource *ctx = calloc(sizeof(local_pif_resource), 1);
    if (ctx == NULL)
        return CMPI_RC_ERR_FAILED;
    ctx->pif = pif;
    ctx->pif_rec = pif_rec;
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
static CMPIrc network_port_set_properties(provider_resource* resource, CMPIInstance *inst)
{
    xen_pif_record *pif_rec = ((local_pif_resource *)resource->ctx)->pif_rec;
    xen_host_record *host_rec = NULL;
    xen_pif_metrics_record *metrics_rec = NULL;
    char buf[MAX_INSTANCEID_LEN];
    char *host_uuid = "NoHost";
    char *host_name = "NoHost";
    CMPIArray *arr = NULL;
    DMTF_CommunicationStatus comm_status = DMTF_CommunicationStatus_Communication_OK;

    if (pif_rec->host->is_record)
        host_rec = pif_rec->host->u.record;
    else
        xen_host_get_record(resource->session->xen, &host_rec, pif_rec->host->u.handle);

    if (host_rec) {
        host_uuid = host_rec->uuid;
        host_name = host_rec->hostname;
    }

    if (pif_rec->metrics->is_record)
        metrics_rec = pif_rec->metrics->u.record;
    else
        xen_pif_metrics_get_record(resource->session->xen, &metrics_rec, pif_rec->metrics->u.handle);
    RESET_XEN_ERROR(resource->session->xen); /* reset errors */

    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, host_uuid, pif_rec->uuid);
    CMSetProperty(inst, "DeviceID",(CMPIValue *)buf, CMPI_chars);
    CMSetProperty(inst, "CreationClassName",(CMPIValue *)"Xen_HostNetworkPort", CMPI_chars);
    CMSetProperty(inst, "SystemCreationClassName",(CMPIValue *)"Xen_HostComputerSystem", CMPI_chars);
    CMSetProperty(inst, "SystemName",(CMPIValue *)host_uuid, CMPI_chars);

    if (metrics_rec) {
        CMSetProperty(inst, "ElementName",(CMPIValue *)metrics_rec->device_id, CMPI_chars);
        CMSetProperty(inst, "Description",(CMPIValue *)metrics_rec->device_name, CMPI_chars);
        if(metrics_rec->speed != 65535) {
            /* 65535 is a special value reserved for NICs that dont have a speed */
            unsigned long long speed = metrics_rec->speed * 1024 * 1024;
            CMSetProperty(inst, "MaxSpeed",(CMPIValue *)&speed, CMPI_uint64);
            CMSetProperty(inst, "Speed",(CMPIValue *)&speed, CMPI_uint64);
        }
        arr = CMNewArray(resource->broker, 2, CMPI_chars, NULL);
        CMSetArrayElementAt(arr, 0, (CMPIValue *)metrics_rec->vendor_name, CMPI_chars);
        CMSetArrayElementAt(arr, 1, (CMPIValue *)metrics_rec->vendor_id, CMPI_chars);
        CMSetProperty(inst, "IdentifyingDescriptions",(CMPIValue *)&arr, CMPI_charsA);
        CMSetProperty(inst, "FullDuplex",(CMPIValue *)&metrics_rec->duplex, CMPI_boolean);
        if (!metrics_rec->carrier)
            comm_status = DMTF_CommunicationStatus_Not_Available;
    }
    else {
        comm_status = DMTF_CommunicationStatus_Not_Available;
    }

    //CMSetProperty(inst, "ActiveMaximumTransmissionUnit",(CMPIValue *)&<value>, CMPI_uint64);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AdditionalAvailability",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "AutoSense",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "Availability",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "AvailableRequestedStates",(CMPIValue *)&arr, CMPI_uint16A);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Host Physical Interface port", CMPI_chars);
    CMSetProperty(inst, "CommunicationStatus",(CMPIValue *)&comm_status, CMPI_uint16);
    //CMSetProperty(inst, "DetailedStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "EnabledDefault",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "EnabledState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ErrorCleared",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "ErrorDescription",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "HealthState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "InstallDate",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "InstanceID",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "LastErrorCode",(CMPIValue *)&<value>, CMPI_uint32);
    //CMSetProperty(inst, "LinkTechnology",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "LocationIndicator",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "MaxQuiesceTime",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "Name",(CMPIValue *)pif_rec->device, CMPI_chars);
    arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)pif_rec->ip, CMPI_chars);
    CMSetProperty(inst, "NetworkAddresses",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "OperatingStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OperationalStatus",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "OtherEnabledState",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherIdentifyingInfo",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "OtherLinkTechnology",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherNetworkPortType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherPortType",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "PermanentAddress",(CMPIValue *)pif_rec->mac, CMPI_chars);
    //CMSetProperty(inst, "PortNumber",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PortType",(CMPIValue *)&<value>, CMPI_uint16);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_uint16, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "PowerManagementCapabilities",(CMPIValue *)&arr, CMPI_uint16A);
    //CMSetProperty(inst, "PowerManagementSupported",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "PowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "PrimaryStatus",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "RequestedSpeed",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "RequestedState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "Status",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "StatusDescriptions",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "StatusInfo",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "SupportedMaximumTransmissionUnit",(CMPIValue *)&pif_rec->mtu, CMPI_uint64);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "TimeOfLastStateChange",(CMPIValue *)&date_time, CMPI_dateTime);
    //CMSetProperty(inst, "TotalPowerOnHours",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "TransitioningToState",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "UsageRestriction",(CMPIValue *)&<value>, CMPI_uint16);
    // 
    if (metrics_rec && !pif_rec->metrics->is_record) {
        xen_pif_metrics_record_free(metrics_rec);
    }
    if (host_rec && !pif_rec->host->is_record) {
        xen_host_record_free(host_rec);
    }
    return CMPI_RC_OK;
}
static CMPIrc rasd_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    xen_pif_record *pif_rec = ((local_pif_resource *)resource->ctx)->pif_rec;
    xen_network_record *net_rec = NULL;
    xen_host_record *host_rec = NULL;
    char *host_uuid = "NoHost";
    char buf[MAX_INSTANCEID_LEN];

    if (pif_rec->network) {
        if (pif_rec->network->is_record)
            net_rec = pif_rec->network->u.record;
        else
            xen_network_get_record(resource->session->xen, &net_rec, pif_rec->network->u.handle);
    }
    if (pif_rec->host->is_record)
        host_rec = pif_rec->host->u.record;
    else
        xen_host_get_record(resource->session->xen, &host_rec, pif_rec->host->u.handle);

    if (host_rec)
        host_uuid = host_rec->uuid;

    CMSetProperty(inst, "Address",(CMPIValue *)pif_rec->mac, CMPI_chars);
    //CMSetProperty(inst, "AddressOnParent",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"count", CMPI_chars);
    //CMSetProperty(inst, "AutomaticAllocation",(CMPIValue *)&<value>, CMPI_boolean);
    //CMSetProperty(inst, "AutomaticDeallocation",(CMPIValue *)&<value>, CMPI_boolean);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Active Virtualization Settings of a Xen Host Network Port", CMPI_chars);
    //CMSetProperty(inst, "ChangeableType",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ConfigurationName",(CMPIValue *)<value>, CMPI_chars);
    CMPIArray *arr = CMNewArray(resource->broker, 1, CMPI_chars, NULL);
    CMSetArrayElementAt(arr, 0, (CMPIValue *)pif_rec->device, CMPI_chars);
    CMSetProperty(inst, "Connection",(CMPIValue *)&arr, CMPI_charsA);
    DMTF_ConsumerVisibility consumer_visibility = DMTF_ConsumerVisibility_Virtualized;
    CMSetProperty(inst, "ConsumerVisibility",(CMPIValue *)&consumer_visibility, CMPI_uint16);
    //CMSetProperty(inst, "Description",(CMPIValue *)value, CMPI_chars);
    //CMSetProperty(inst, "DesiredVLANEndpointMode",(CMPIValue *)&<value>, CMPI_uint16);
    CMSetProperty(inst, "ElementName",(CMPIValue *)host_uuid, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "HostResource",(CMPIValue *)&arr, CMPI_charsA);
    _CMPICreateNewDeviceInstanceID(buf, sizeof(buf), host_uuid, pif_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);
    /* IP configuration for this NIC */
    CMSetProperty(inst, "IPConfigurationMode",(CMPIValue *)&(pif_rec->ip_configuration_mode), CMPI_uint8);
    if (pif_rec->ip)
        CMSetProperty(inst, "IPAddress", (CMPIValue *)pif_rec->ip, CMPI_chars);
    if (pif_rec->netmask)
        CMSetProperty(inst, "IPSubnetMask", (CMPIValue *)pif_rec->netmask, CMPI_chars);
    if (pif_rec->gateway)
        CMSetProperty(inst, "IPGateway", (CMPIValue *)pif_rec->gateway, CMPI_chars);
    if (pif_rec->dns)
        CMSetProperty(inst, "DNS", (CMPIValue *)pif_rec->dns, CMPI_chars);
    CMSetProperty(inst, "Management", (CMPIValue *)&pif_rec->management, CMPI_boolean);
    /* find the management purpose stuck inside of the other_config field */
    char *purpose = xen_utils_get_from_string_string_map(pif_rec->other_config, "management_purpose");
    if (purpose)
        CMSetProperty(inst, "ManagementPurpose", (CMPIValue *)purpose, CMPI_chars);

    //CMSetProperty(inst, "Limit",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "MappingBehavior",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OtherEndpointMode",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    if (pif_rec->bond_slave_of) {
        if (pif_rec->bond_slave_of->is_record)
            CMSetProperty(inst, "Parent",(CMPIValue *)pif_rec->bond_slave_of->u.record->uuid, CMPI_chars);
        else {
            char *master_uuid = NULL;
            if (xen_pif_get_uuid(resource->session->xen, &master_uuid, pif_rec->bond_slave_of->u.handle)) {
                CMSetProperty(inst, "Parent",(CMPIValue *)master_uuid, CMPI_chars);
                free(master_uuid);
            }
        }
    }
    CMSetProperty(inst, "PoolID",(CMPIValue *)host_rec->uuid, CMPI_chars);
    //CMSetProperty(inst, "Reservation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    int res_type = DMTF_ResourceType_Ethernet_Adapter;
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&res_type, CMPI_uint16);
    unsigned long long count = 1;
    CMSetProperty(inst, "VirtualQuantity",(CMPIValue *)&count, CMPI_uint64);
    CMSetProperty(inst, "VirtualQuantityUnits",(CMPIValue *)"count", CMPI_chars);
    if (pif_rec->vlan != -1)
        CMSetProperty(inst, "VlanTag",(CMPIValue *)&pif_rec->vlan, CMPI_uint64);
    if (net_rec)
        CMSetProperty(inst, "VirtualSwitch",(CMPIValue *)net_rec->uuid, CMPI_chars);
    //CMSetProperty(inst, "Weight",(CMPIValue *)&<value>, CMPI_uint32);

    if (net_rec && !pif_rec->network->is_record) {
        xen_network_record_free(net_rec);
    }
    if (host_rec && !pif_rec->host->is_record) {
        xen_host_record_free(host_rec);
    }
    return CMPI_RC_OK;
}

static CMPIrc metrics_set_properties(
    const CMPIBroker *broker,
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    xen_pif_record *pif_rec = ((local_pif_resource *)resource->ctx)->pif_rec;
    xen_host_record *host_rec = NULL;
    char buf[MAX_INSTANCEID_LEN];
    char *host_uuid = "NoHost";
    xen_host host = NULL;
    CMPIrc statusrc = CMPI_RC_OK;

    if (pif_rec->host->is_record) {
        host_rec = pif_rec->host->u.record;
        xen_host_get_by_uuid(resource->session->xen, &host, host_rec->uuid);
    }
    else {
        host = pif_rec->host->u.handle;
        xen_host_get_record(resource->session->xen, &host_rec, pif_rec->host->u.handle);
    }

    if (!host_rec) {
        statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
        goto exit;
    }

    host_uuid = host_rec->uuid;

    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, host_uuid, pif_rec->uuid);
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    snprintf(buf, MAX_INSTANCEID_LEN, "%sDef", resource->classname);
    CMSetProperty(inst, "MetricDefinitionId",(CMPIValue *)buf, CMPI_chars);

    //CMSetProperty(inst, "BreakdownDimension",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "BreakdownValue",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Host Network Port metrics", CMPI_chars);
    //CMPIDateTime *date_time = xen_utils_time_t_to_CMPIDateTime(_BROKER, &<time_value>);
    //CMSetProperty(inst, "Duration",(CMPIValue *)&date_time, CMPI_dateTime);
    CMSetProperty(inst, "ElementName",(CMPIValue *)host_rec->uuid, CMPI_chars);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    CMSetProperty(inst, "MeasuredElementName",(CMPIValue *)host_rec->name_label, CMPI_chars);
    // 
    double io_kbps = 0;
    if (strcmp(resource->classname, rx_metrics_cn) == 0)
        snprintf(buf, MAX_INSTANCEID_LEN, "pif_%s_rx", pif_rec->device);
    else
        snprintf(buf, MAX_INSTANCEID_LEN, "pif_%s_tx", pif_rec->device);
    CMSetProperty(inst, "Description",(CMPIValue *)buf, CMPI_chars);
    xen_host_query_data_source(resource->session->xen, &io_kbps, host, buf);
    snprintf(buf, MAX_INSTANCEID_LEN, "%f", io_kbps);
    CMSetProperty(inst, "MetricValue", (CMPIValue *)buf, CMPI_chars);

    CMPIDateTime *date_time = xen_utils_CMPIDateTime_now(broker);
    CMSetProperty(inst, "TimeStamp",(CMPIValue *)&date_time, CMPI_dateTime);
    bool vol=true;
    CMSetProperty(inst, "Volatile",(CMPIValue *)&vol, CMPI_boolean);
    // 

exit:
    if (host_rec && !pif_rec->host->is_record)
        xen_host_record_free(host_rec);
    if (host && pif_rec->host->is_record)
        xen_host_free(host);

    return statusrc;
}

static CMPIrc xen_resource_set_properties(
    provider_resource *resource, 
    CMPIInstance *inst
    )
{
    if (xen_utils_class_is_subclass_of(resource->broker, hnp_cn, resource->classname))
        network_port_set_properties(resource, inst);
    else if (xen_utils_class_is_subclass_of(resource->broker, rasd_cn, resource->classname))
        rasd_set_properties(resource, inst);
    else
        metrics_set_properties(resource->broker, resource, inst);
    return CMPI_RC_OK;
}

/* Setup the function table for the instance provider */
XenInstanceMIStub(Xen_HostNetworkPort)

/*****************************************************************************
   Helper functions relating to parsing HostNetworkPort RASD
******************************************************************************/
/******************************************************************************
 * Convert a Network Port RASD object into a Xen PIF record
 * The same RASD object might match multilpe PIFs on different hosts,
 * Hence the PIF record SET.
 *
 * @param in broker - CMPI Factory broker
 * @param in session - xen session handle
 * @param in nic_rads - the RASD to convert to convert to a PIF record 
 * @param in/out pif_rec_set - the pif record set to add the new record to
 * @param out bonded_set - if the pif set represents a set of bonded PIFs or not
 * @param in/out status - CMPI status of the opeartion
 *
 * @returns true if success, false if failed
******************************************************************************/
    static bool network_rasd_to_pif_set(
    const CMPIBroker* broker,
    xen_utils_session *session,
    CMPIInstance *nic_rasd,
    xen_pif_record_set **pif_rec_set,
    bool *bonded_set,
    CMPIStatus *status)
{
    CMPIData arr, prop;
    char *devices = NULL;
    xen_pif_set *all_pifs = NULL;
    int64_t vlan_id = -1;
    int i=0;
    char buf[100];
    memset(buf, 0, sizeof(buf));
    char *error_msg = "ERROR: Unknown error";
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    xen_string_set * device_list = NULL;
    char* pif_uuid = NULL;
    char *ip_address = NULL, *gateway = NULL, *netmask = NULL, *dns = NULL, *purpose=NULL;
    int ip_mode = -1;

    prop = CMGetProperty(nic_rasd, "InstanceID", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type == CMPI_string)) {
        /* This identifies an existing PIF (for remove/modify) */
        char *instanceid = CMGetCharPtr(prop.value.string);
        if (instanceid) {
            if (_CMPIStrncpyDeviceNameFromID(buf, instanceid, sizeof(buf)))
                pif_uuid = strdup(buf);
        }
    }
    else {
        /* This only identifies a setting for creation, get the connection information */
        arr = CMGetProperty(nic_rasd, "Connection", status);
        if ((status->rc != CMPI_RC_OK) || CMIsNullValue(arr) || !CMIsArray(arr)) {
            error_msg = "ERROR: Invalid parameter 'Connection'";
            goto Exit;
        }
        prop = CMGetArrayElementAt(arr.value.array, 0, status);
        if ((status->rc != CMPI_RC_OK) || CMIsNullValue(prop) || (prop.type != CMPI_string)) {
            error_msg = "ERROR: Invalid array parameter 'Connection'";
            goto Exit;
        }
        devices = CMGetCharPtr(prop.value.string);
        device_list = xen_utils_copy_to_string_set(devices, ",");
    }

    /* Get all the modifyable properties here */
    prop = CMGetProperty(nic_rasd, "VlanTag", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type & CMPI_INTEGER))
        vlan_id = prop.value.uint64;
    prop = CMGetProperty(nic_rasd, "IPConfigurationMode", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type & CMPI_INTEGER))
        ip_mode = prop.value.uint8;
    prop = CMGetProperty(nic_rasd, "IPAddress", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type == CMPI_string))
        ip_address = CMGetCharPtr(prop.value.string);
    prop = CMGetProperty(nic_rasd, "IPSubnetMask", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type == CMPI_string))
        netmask = CMGetCharPtr(prop.value.string);
    prop = CMGetProperty(nic_rasd, "IPGateway", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type == CMPI_string))
        gateway = CMGetCharPtr(prop.value.string);
    prop = CMGetProperty(nic_rasd, "DNS", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type == CMPI_string))
        dns = CMGetCharPtr(prop.value.string);
    prop = CMGetProperty(nic_rasd, "ManagementPurpose", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type == CMPI_string))
        purpose = CMGetCharPtr(prop.value.string);

    /* got to have either the pif uuid or a list of interface names (like eth0 etc) to continue */
    if (!device_list && !pif_uuid)
        goto Exit;

    if (pif_uuid) {
        /* RASD specified a unique PIF, ignore the Connection data */
        xen_pif_record *pif_rec = NULL;
        xen_pif pif = NULL;
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Finding specified pif %s", pif_uuid));
        if (xen_pif_get_by_uuid(session->xen, &pif, pif_uuid)) {
            if (xen_pif_get_record(session->xen, &pif_rec, pif)) {
                // modify the appropriate members of the pif record here
                if (vlan_id != -1)
                    pif_rec->vlan = vlan_id;
                if (ip_mode != -1)
                    pif_rec->ip_configuration_mode = ip_mode;
                if(ip_mode != 0) {
                    /* Caller wishes to set the IP configuration */
                    if (ip_address != NULL)
                        pif_rec->ip = strdup(ip_address);
                    if (netmask != NULL)
                        pif_rec->netmask = strdup(netmask);
                    if (gateway != NULL)
                        pif_rec->gateway = strdup(gateway);
                    if (dns != NULL)
                        pif_rec->dns = strdup(dns);
                    if (purpose != NULL)
                        xen_utils_add_to_string_string_map("management_purpose", purpose, &(pif_rec->other_config));
                }
                else {
                    /* Caller is setting IP configuration of interface to None */
                    pif_rec->ip = strdup("");
                    pif_rec->netmask = strdup("");
                    pif_rec->gateway = strdup("");
                    pif_rec->dns = strdup("");
                }
                ADD_DEVICE_TO_LIST((*pif_rec_set), pif_rec, xen_pif_record);
            }
            xen_pif_free(pif);
        }
    }
    else if (device_list) {
        /* the RASD specified a list of pifs to look for based on connection (eth1, eth2 etc) data */
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_INFO, ("Looking for pifs specified by the %d devices", device_list->size));

        if (!xen_pif_get_all(session->xen, &all_pifs))
            goto Exit;

        /* more than one ehternet interface has been specified */
        if (device_list->size > 1)
            *bonded_set = true;

        /* Check to see if we can find a PIF with the device name specified, if so, 
         * get its pif record (which includes host etc) and update the record with 
         * any data (VLAN) passed in the RASD 
         */
        i=all_pifs->size;
        while (i>0) {
            xen_pif_record *pif_rec = NULL;
            bool added_to_piflist = false;
            if (xen_pif_get_record(session->xen, &pif_rec, all_pifs->contents[i-1])) {
                int i;
                if (pif_rec->device) {
                    for (i=0; i<device_list->size; i++) {
                        if ((strcmp(pif_rec->device, device_list->contents[i]) == 0) &&
                            pif_rec->vlan == -1) { 
                            /* Dont include ones that have a VLAN id already, they cant be used to create a new VLAN */
                            /* modify the appropriate members of the pif record here */
                            pif_rec->vlan = vlan_id;
                            ADD_DEVICE_TO_LIST((*pif_rec_set), pif_rec, xen_pif_record);
                            added_to_piflist = true;
                            break;
                        }
                    }
                }
                if (!added_to_piflist)
                    xen_pif_record_free(pif_rec);
            }
            else
                goto Exit;
            i--;
        }
    }
    if (*pif_rec_set == NULL) {
        snprintf(buf, sizeof(buf)/sizeof(buf[0]), "Host network adapter %s specified in the RASD, could not be found.", devices);
        error_msg = buf;
        statusrc = CMPI_RC_ERR_FAILED;
    }
    else {
        RESET_XEN_ERROR(session->xen);
        statusrc = CMPI_RC_OK;
    }

    Exit:
    if (pif_uuid)
        free(pif_uuid);
    if (all_pifs)
        xen_pif_set_free(all_pifs);
    if (device_list)
        xen_string_set_free(device_list);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return(statusrc == CMPI_RC_OK);
}
/******************************************************************************
 * Parse a Nic RASD instance array and return back a set of PIF records
 *
 * @param in broker - CMPI Factory broker
 * @param in session - xen session handle
 * @param in setting_data - raw CMPI data that represents the RASD array
 * @param in/out pif_rec_set - the pif record set to add the new record to
 * @param out bonded_set - if the pif set represents a set of bonded PIFs or not
 * @param in/out status - CMPI status of the opeartion
 *
 * @returns true if success, false if failed
******************************************************************************/
bool host_network_port_rasd_parse(
    const CMPIBroker *broker, 
    xen_utils_session *session,
    CMPIData *setting_data,
    xen_pif_record_set **pif_rec_set,
    bool *bonded_set,
    CMPIStatus *status
    )
{
    /* Convert one RASD setting to its respecitve xen record. */
    CMPIInstance *instance = NULL;
    CMPIObjectPath *objectpath = NULL;
    char *settingclassname = NULL;
    DMTF_ResourceType resourceType = DMTF_ResourceType_Ethernet_Connection;
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    char *error_msg = "ERROR: Error parsing the RASD";

    if (!xen_utils_get_cmpi_instance(broker, setting_data, &objectpath, &instance) || 
        !instance) {
        error_msg = "ERROR: Failed to parse the Xen_HostNetworkPortSettingData instance";
        goto Exit;
    }

    settingclassname = CMGetCharPtr(CMGetClassName(objectpath, NULL));
    CMPIData prop = CMGetProperty(instance, "ResourceType", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(prop) && (prop.type & CMPI_INTEGER))
        resourceType = prop.value.uint16;

    if (xen_utils_class_is_subclass_of(broker, settingclassname, "CIM_EthernetPortAllocationSettingData") && 
        ((resourceType == DMTF_ResourceType_Ethernet_Connection) ||
        (resourceType == DMTF_ResourceType_Ethernet_Adapter))) {
        if (network_rasd_to_pif_set(broker, session, instance, pif_rec_set, bonded_set, status) && pif_rec_set)
            statusrc = CMPI_RC_OK;
    }
    else {
        error_msg = "ERROR: CIM instance is not of type CIM_EthernetPortAllocationSettingData or is of  the wrong ResourceType";
    }

    Exit:
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);
    return(statusrc == CMPI_RC_OK);
}
