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

#include <stdlib.h>
#include <inttypes.h>

#include "xen_utils.h"
#include "provider_common.h"
#include <cmpidt.h>
#include <cmpimacs.h>
#include "cmpilify.h"
#include "cmpitrace.h"
#include "RASDs.h"
/******************************************************************************
* network_rasd_to_vif
*
* This function attempts to parse a Xen_NetworkPort CIM instance and populate a new
* VIF record. It also gets the record to underlying network as pointed to by the
* PoolID field in the CIM instance.
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
int network_rasd_to_vif(
    const CMPIBroker* broker,
    xen_utils_session *session,
    CMPIInstance *nic_rasd,
    bool new_vif,
    xen_vif_record **vif_rec,
    CMPIStatus *status)
{
    CMPIData propertyvalue;
    char * mac = NULL;
    char * networkuuid = NULL;
    char * device = NULL;
    char * vif_uuid = NULL;
    char *vlanid = NULL;
    char *error_msg = "ERROR: Error parsing the NetworkPortSettingData";
    CMPIrc statusrc = CMPI_RC_ERR_INVALID_PARAMETER;
    *vif_rec = NULL;
    char buf[MAX_INSTANCEID_LEN];

    /* Set the domain config data from the Xen_NetworkPortSettingData. */
    propertyvalue = CMGetProperty(nic_rasd, "Address", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        mac = strdup(CMGetCharPtr(propertyvalue.value.string));

    /* Get the instance ID which has the device's UUID in it, if its available
      This will be usd during deletes and not used during create*/
    propertyvalue = CMGetProperty(nic_rasd, "InstanceID", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) && (propertyvalue.type == CMPI_string)) {
        _CMPIStrncpyDeviceNameFromID(buf,CMGetCharPtr(propertyvalue.value.string), sizeof(buf)/sizeof(buf[0]));
        vif_uuid = strdup(buf);
    } else {
        if(!new_vif) {
            /* expecting to find reference to an existing vif */
            error_msg = "ERROR: No InstanceID specified in Xen_NetworkPortSettingData";
            goto Error;
        }
    }

    /* Network to connect to - PoolID proeprty matches the PoolID proeprty of the associated network pool */
    CMPIData networkproperty = CMGetProperty(nic_rasd, "PoolID", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(networkproperty) && 
       (networkproperty.type == CMPI_string)) {
        /* This matches the Name property of the virtual switch */
        networkuuid = strdup(CMGetCharPtr(networkproperty.value.string));
    }
    if (new_vif && (networkuuid == NULL)) {
        /* expecting to find some reference to the network */
        error_msg = "ERROR: No PoolID specified in Xen_NetworkPortSettingData";
        goto Error;
    }

    /* The connection identifies the VLAN to connect it to */
    CMPIData arr;
    arr = CMGetProperty(nic_rasd, "Connection", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(arr)) {
        propertyvalue = CMGetArrayElementAt(arr.value.array, 1, status);
        if (status->rc == CMPI_RC_OK && !CMIsNullValue(propertyvalue)) {
            vlanid = CMGetCharPtr(propertyvalue.value.string);
        }
    }
    else {
        /* could be in the vlantag property as well */
        propertyvalue = CMGetProperty(nic_rasd, "VlanTag", status);
        if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue) 
            && (propertyvalue.type == CMPI_string)) {
            vlanid = CMGetCharPtr(propertyvalue.value.string);
        }
    }

    /* The devicename to connect this adapter to is available on the AddressOnParent
     property. It could be NULL, in which case we get the next device to connect it on */
    propertyvalue = CMGetProperty(nic_rasd, "AddressOnParent", status);
    if ((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
        device = strdup(CMGetCharPtr(propertyvalue.value.string));

    /* Create the device record from the information we have */
    *vif_rec = xen_vif_record_alloc();
    if (*vif_rec == NULL) {
        error_msg = "ERROR: Unable to malloc memory";
        goto Error;
    }
    (*vif_rec)->mac = mac;
    if (vif_uuid)
        (*vif_rec)->uuid = vif_uuid;
    if (device)
        (*vif_rec)->device = device;

    if (networkuuid) {
        xen_network network_handle;
        if (!xen_network_get_by_uuid(session->xen, &network_handle, networkuuid)) {
            error_msg = "ERROR: Unable to find network based on the PoolID in the Xen_NetworkPortSettingData parameter";
            goto Error;
        }
        xen_network_record_opt* network_opt = xen_network_record_opt_alloc();
        network_opt->u.handle = network_handle;
        (*vif_rec)->network = network_opt;
        free(networkuuid);
    }

#if XENAPI_VERSION > 400
    (*vif_rec)->other_config = xen_string_string_map_alloc(0);
#endif
    (*vif_rec)->qos_algorithm_params = xen_string_string_map_alloc(0);
    return 1;

    Error:

    if (device) {
        free(device);
        if (*vif_rec)
            (*vif_rec)->device = NULL;
    }
    if (mac) {
        free(mac);
        if (*vif_rec)
            (*vif_rec)->mac = NULL;
    }
    if (networkuuid)
        free(networkuuid);
    xen_utils_set_status(broker, status, statusrc, error_msg, session->xen);

    return 0;
}
/*
* network_rasd_from_vif
*
* Parses a VIF and constructs a new Xen_NetworkPort CIM instance.
* Could get down to the xen Network and PIF objects for various
* bits and pieces of information.
*
* Returns 1 on Success and 0 on failure.
*/
int network_rasd_from_vif(
    const CMPIBroker* broker,
    xen_utils_session *session,
    xen_vif_record* vif_rec,
    CMPIInstance* inst
    )
{
    char nic_config_info[512];
    char buf[MAX_INSTANCEID_LEN];
    unsigned long long nics = 1;
    int alloctype = 1;
    DMTF_ResourceType resourceType = DMTF_ResourceType_Ethernet_Connection;
    DMTF_ConsumerVisibility consumerVisibility = DMTF_ConsumerVisibility_Virtualized;
    CMPIArray *arr = NULL;

    nic_config_info[0] = '\0';

    xen_vm_record_opt *vm_rec_opt = vif_rec->vm;
    xen_network_record_opt *network_opt = vif_rec->network;
    xen_network_record *network_rec = NULL;
    xen_pif_record *pif_rec = NULL;

    /* get the VIF UUID */
    if (vm_rec_opt->is_record)
        _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec_opt->u.record->uuid, vif_rec->uuid);
    else {
        char *uuid = NULL;
        if (!xen_vm_get_uuid(session->xen, &uuid, vm_rec_opt->u.handle)) {
            xen_utils_trace_error(session->xen, __FILE__, __LINE__);
            return CMPI_RC_ERR_FAILED;
        }
        _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, uuid, vif_rec->uuid);
        free(uuid);
    }
    /* Key property */
    CMSetProperty(inst, "InstanceID",(CMPIValue *)buf, CMPI_chars);

    xen_network network = NULL;
    xen_pif_set *pif_set = NULL;
    if (network_opt->is_record) {
        xen_network_get_by_uuid(session->xen, &network, network_opt->u.record->uuid);
        network_rec = network_opt->u.record;
    }
    else {
        network = network_opt->u.handle;
        xen_network_get_record(session->xen, &network_rec, network);
    }
    xen_network_get_pifs(session->xen, &pif_set, network);
    if (pif_set && pif_set->size > 0)
        xen_pif_get_record(session->xen, &pif_rec, pif_set->contents[0]);

    CMSetProperty(inst, "Address", (CMPIValue *)vif_rec->mac, CMPI_chars);
    CMSetProperty(inst, "AddressOnParent", (CMPIValue *)vif_rec->device, CMPI_chars);
    CMSetProperty(inst, "AllocationUnits",(CMPIValue *)"count", CMPI_chars);
    CMSetProperty(inst, "AutomaticAllocation" , (CMPIValue *)&alloctype, CMPI_boolean);
    CMSetProperty(inst, "AutomaticDeallocation" , (CMPIValue *)&alloctype, CMPI_boolean);
    CMSetProperty(inst, "AutoGeneratedMAC", (CMPIValue *)&vif_rec->mac_autogenerated, CMPI_boolean);

    CMSetProperty(inst, "Caption",(CMPIValue *)"Active virtualization settings for a virtual ethernet connection", CMPI_chars);
    CMSetProperty(inst, "ConsumerVisibility" , (CMPIValue *)&consumerVisibility, CMPI_uint16);

    //CMSetProperty(inst, "DesiredVLANEndpointMode",(CMPIValue *)&<value>, CMPI_uint32);
    //CMSetProperty(inst, "InstanceID",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Limit",(CMPIValue *)&nics, CMPI_uint64);

    //CMSetProperty(inst, "MappingBehavior",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Parent",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "Reservation",(CMPIValue *)&nics, CMPI_uint64);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    CMSetProperty(inst, "ResourceType",(CMPIValue *)&resourceType, CMPI_uint16);
    CMSetProperty(inst, "VirtualQuantity",(CMPIValue *)&nics, CMPI_uint64);

    CMSetProperty(inst, "VirtualQuantityUnits",(CMPIValue *)"count", CMPI_chars);
    //CMSetProperty(inst, "Weight",(CMPIValue *)&<value>, CMPI_uint32);

    if (network_rec) {
        CMSetProperty(inst, "Description",(CMPIValue *)network_rec->name_label, CMPI_chars);
        CMSetProperty(inst, "PoolID", (CMPIValue *)network_rec->uuid, CMPI_chars);
        CMSetProperty(inst, "ElementName",(CMPIValue *)network_rec->bridge, CMPI_chars);
    }
    if (pif_rec) {
        arr = CMNewArray(broker, 1, CMPI_chars, NULL);
        CMSetArrayElementAt(arr, 0, (CMPIValue *)pif_rec->device, CMPI_chars);
        CMSetProperty(inst, "HostResource",(CMPIValue *)&arr, CMPI_charsA);
        // Connection is supposed to contain an array of VLANIDs/Network UUIDs
        if(pif_rec->vlan != -1) {
            snprintf(buf, sizeof(buf), "%" PRId64, pif_rec->vlan);
            arr = CMNewArray(broker, 1, CMPI_chars, NULL);
            CMSetArrayElementAt(arr, 0, (CMPIValue *) buf, CMPI_chars);
            CMSetProperty(inst, "Connection", &arr, CMPI_charsA);
        }
        CMSetProperty(inst, "VlanTag",(CMPIValue *)&pif_rec->vlan, CMPI_uint64);
    }

    if (pif_set)
        xen_pif_set_free(pif_set);
    if (pif_rec)
        xen_pif_record_free(pif_rec);
    if (network_opt->is_record)
        xen_network_free(network);
    else
        xen_network_record_free(network_rec);

    return 1;
}

/* updates a vif record with settings from a template */
void _merge_vif_record(
    xen_vif_record *vif_rec,            /* in/out */
    xen_vif_record *vif_rec_template    /* in */
    )
{
    if (vif_rec_template->mac 
        && vif_rec_template->mac != '\0' 
        && strcmp(vif_rec_template->mac, vif_rec->mac) != 0) {
        /* This swap is to make sure the string gets freed by the proper deallocator fn */
        char *tmp = vif_rec->mac;
        vif_rec->mac = vif_rec_template->mac;
        vif_rec_template->mac = tmp;
    }
    if (vif_rec_template->device 
        && vif_rec_template->device != '\0' 
        && strcmp(vif_rec_template->device, vif_rec->device) != 0) {
        /* This swap is to make sure the string gets freed by the proper deallocator fn */
        char *tmp = vif_rec->device;
        vif_rec->device = vif_rec_template->device;
        vif_rec_template->device = tmp;
    }
    if (vif_rec_template->mtu != 0
        && vif_rec_template->mtu != vif_rec->mtu) {
        vif_rec->mtu = vif_rec_template->mtu;
    }
}

/*
 * This function modifies the VIF based on the newly specified settings (other_config only)
*/
int network_rasd_modify(
    xen_utils_session* session,
    xen_vif_record *vif_rec_template
    )
{
    xen_vif vif = NULL;
    xen_vif_record *vif_rec = NULL;
    int rc = 0;

    if (!xen_vif_get_by_uuid(session->xen, &vif, vif_rec_template->uuid)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Couldnt find VIF %s",vif_rec_template->uuid));
        return rc;
    }

    if (!xen_vif_get_record(session->xen, &vif_rec, vif)) {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Couldnt get current VIF record for %s",vif_rec_template->uuid));
        goto Exit;
    }

    /* Deleteing and recreating under the covers is not allowed since the UUID changes
       and we end up with a CIM object where the key changes upon modify */
    if(vif_rec_template->other_config && vif_rec_template->other_config->size > 0) {
        int i;
        for(i=0; i<vif_rec_template->other_config->size; i++) {
            xen_utils_add_to_string_string_map(vif_rec_template->other_config->contents[i].key,
                                               vif_rec_template->other_config->contents[i].val,
                                               &vif_rec->other_config);

        }
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,("--- Setting the new vif config for %s", 
                                               vif_rec_template->uuid));
        xen_vif_set_other_config(session->xen, vif, vif_rec->other_config);
    }
    //if(vif_rec_template->qos_algorithm_type)
    //    xen_vif_set_qos_algorithm_type(session->xen, vif, vif_rec_template->qos_algorithm_type);
    //if(vif_rec_template->qos_algorithm_params)
    //    xen_vif_set_qos_algorithm_params(session->xen, vif, vif_rec_template->qos_algorithm_params);
    //if(vif_rec_template->qos_algorithm_params)
    //    xen_vif_add_to_qos_algorithm_params(session->xen, vif, key, value);
    //if(vif_rec_template->qos_algorithm_params)
    //    xen_vif_remove_from_qos_algorithm_params(session->xen, vif, key);
    //
    rc = 1;

    Exit:
    if (vif)
        xen_vif_free(vif);
    if (vif_rec)
        xen_vif_record_free(vif_rec);
    return rc;
}

CMPIObjectPath *network_rasd_create_ref(
    const CMPIBroker *broker,
    const char *name_space,
    xen_utils_session *session,
    xen_vm_record *vm_rec,
    xen_vif vif
    )
{
    char inst_id[MAX_INSTANCEID_LEN];
    char* vif_uuid = NULL;
    CMPIObjectPath *result_setting = NULL;

    if (xen_vif_get_uuid(session->xen, &vif_uuid, vif)) {
        result_setting = CMNewObjectPath(broker, name_space, "Xen_NetworkPortSettingData", NULL);
        if(result_setting) {
            _CMPICreateNewDeviceInstanceID(inst_id, MAX_INSTANCEID_LEN, vm_rec->uuid, vif_uuid);
            CMAddKey(result_setting, "InstanceID", (CMPIValue *)inst_id, CMPI_chars);
        }
        free(vif_uuid);
    }
    return result_setting;
}

