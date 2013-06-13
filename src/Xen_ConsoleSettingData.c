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
#include "xen_utils.h"
#include "RASDs.h"

/******************************************************************************
* console_rasd_to_xen_console_rec
*
* Parses a Xen_Console CIM instance and consructs a new console record.
*
* Returns 1 on Success and 0 on failure.
******************************************************************************/
int console_rasd_to_xen_console_rec(
    const CMPIBroker *broker,
    CMPIInstance *con_rasd,
    xen_console_record **con_rec,
    CMPIStatus *status)
{
    CMPIStatus local_status = {CMPI_RC_OK, NULL};
    CMPIData propertyvalue;

    *con_rec = xen_console_record_alloc();
    if(*con_rec == NULL)
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Cannot malloc memory for console record"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM, "Unable to malloc memory");
        goto Error;
    }

    propertyvalue = CMGetProperty(con_rasd, "Protocol", &local_status);
    if((local_status.rc != CMPI_RC_OK) || CMIsNullValue(propertyvalue))
    {
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- No protocol field specified in console setting data"));
        CMSetStatusWithChars(broker, status,  CMPI_RC_ERR_INVALID_PARAMETER, "No protocol specified in console setting data");
        return 0;
    }
    switch(propertyvalue.value.uint16)
    {
    case 0:
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG,
            ("--- VT100 protocol selected"));
        (*con_rec)->protocol = XEN_CONSOLE_PROTOCOL_VT100;
        break;
    case 1:
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG,
            ("--- RFB protocol selected"));
        (*con_rec)->protocol = XEN_CONSOLE_PROTOCOL_RFB;
        break;
    case 2:
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_DEBUG,
            ("--- RDP protocol selected"));
        (*con_rec)->protocol = XEN_CONSOLE_PROTOCOL_RDP;
        break;
    default:
        _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
            ("--- Invalid protocol specified in console setting data"));
        CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER, "Invalid protocol specified in console setting data");
        goto Error;
    }

    /* Get the instance ID which has the device's UUID in it, if its available
      This will be usd during deletes and not used during create*/
    propertyvalue = CMGetProperty(con_rasd, "InstanceID", status);
    if((status->rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
    {
        char buf[MAX_INSTANCEID_LEN];
        _CMPIStrncpyDeviceNameFromID(buf,
            CMGetCharPtr(propertyvalue.value.string),
            sizeof(buf)/sizeof(buf[0]));
        (*con_rec)->uuid = strdup(buf);
    }

    /*
    * Get any additional config from ConsoleConfigInfo.
    * Expected syntax is "key=value,key=value"
    */
    propertyvalue = CMGetProperty(con_rasd, "ConsoleConfigInfo", &local_status);
    if((local_status.rc == CMPI_RC_OK) && !CMIsNullValue(propertyvalue))
    {
        /* Count number of config items */
        int num_items = 0;
        char *next_tok;
        char *string = strdup(CMGetCharPtr(propertyvalue.value.string));
        char *tok = strtok_r(string, ",", &next_tok);
        while(tok)
        {
            num_items++;
            tok = strtok_r(NULL, ",", &next_tok);
        }
        free(string);

        xen_string_string_map *con_params = xen_string_string_map_alloc(num_items);
        if(con_params == NULL)
        {
            _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                ("--- Cannot malloc memory for console options"));
            CMSetStatusWithChars(broker, status, CMPI_RC_ERROR_SYSTEM, "Unable to malloc memory");
            goto Error;
        }

        /*
        * Go back through the options and populate the string map.
        */
        string = strdup(CMGetCharPtr(propertyvalue.value.string));
        tok = strtok_r(string, ",", &next_tok);
        /* If tok is NULL, then string contains only 1 key/value pair */
        if(tok == NULL)
            tok = string;
        int i = 0;
        while(tok)
        {
            char *val = strchr(tok, '=');
            if(val == NULL)
            {
                _SBLIM_TRACE(_SBLIM_TRACE_LEVEL_ERROR,
                    ("--- Invalid console option specified in console setting data"));
                CMSetStatusWithChars(broker, status, CMPI_RC_ERR_INVALID_PARAMETER, "Invalid console option specified in console setting data");
                xen_string_string_map_free(con_params);
                free(string);
                goto Error;
            }
            *val = '\0';
            val++;
            con_params->contents[i].key = strdup(tok);
            con_params->contents[i].val = strdup(val);
            i++;
            tok = strtok_r(NULL, ",", &next_tok);
        }

        (*con_rec)->other_config = con_params;
        free(string);
    }

    return 1;
 Error:
    if(*con_rec)
        xen_console_record_free(*con_rec);
    return 0;
}
/*
* console_rec_to_console_rasd
*
* Parses a xen console object and constructs a Xen_Console CIM instance.
*
* Returns 1 on Success and 0 on failure.
*/
int xen_console_rec_to_console_rasd(
    const CMPIBroker* broker,
    xen_utils_session *session,
    CMPIInstance *inst,
    xen_console_record* con_rec
    )
{
    xen_vm_record_opt *vm_rec_opt = con_rec->vm;
    char buf[MAX_INSTANCEID_LEN];
    DMTF_ResourceType type = DMTF_ResourceType_Graphics_controller;
    DMTF_ConsumerVisibility consumerVisibility = DMTF_ConsumerVisibility_Virtualized;
    uint64_t consoles = 1;
    xen_vm_record *vm_rec = NULL;
    int alloctype = 1;
    char *vm_uuid = "NoHost";
    char *vm_name = "No VM information available";
    char *vm_desc = "No VM description available";

    if(vm_rec_opt->is_record) {
        _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_rec_opt->u.record->uuid, con_rec->uuid);
    }
    else {
        if(xen_vm_get_record(session->xen, &vm_rec, vm_rec_opt->u.handle)) {
            vm_uuid = vm_rec->uuid;
            vm_name = vm_rec->name_label;
            vm_desc = vm_rec->name_description;
        }
    }
    _CMPICreateNewDeviceInstanceID(buf, MAX_INSTANCEID_LEN, vm_uuid, con_rec->uuid);
    CMSetProperty(inst, "InstanceID", (CMPIValue *)buf, CMPI_chars);

    /* Set the CMPIInstance properties from the resource data. */
    CMSetProperty(inst, "Caption",(CMPIValue *)"Xen Console Settings", CMPI_chars);
    CMSetProperty(inst, "Description",(CMPIValue *)vm_desc, CMPI_chars);
    CMSetProperty(inst, "ElementName", (CMPIValue *)vm_name, CMPI_chars);
    CMSetProperty(inst, "ResourceType", (CMPIValue *)&type, CMPI_uint16);
    CMSetProperty(inst, "ConsumerVisibility", (CMPIValue *)&consumerVisibility, CMPI_uint16);
    CMSetProperty(inst, "Reservation", (CMPIValue *)&consoles, CMPI_uint64);
    CMSetProperty(inst, "Limit", (CMPIValue *)&consoles, CMPI_uint64);
    CMSetProperty(inst, "VirtualQuantity", (CMPIValue *)&consoles, CMPI_uint64);
    CMSetProperty(inst, "AllocationUnits", (CMPIValue *)"count", CMPI_chars);
    CMSetProperty(inst, "VirtualQuantityUnits",(CMPIValue *)"count", CMPI_chars);
    CMSetProperty(inst, "AutomaticAllocation", (CMPIValue *)&alloctype, CMPI_boolean);
    CMSetProperty(inst, "AutomaticDeallocation", (CMPIValue *)&alloctype, CMPI_boolean);
    CMSetProperty(inst, "Protocol", (CMPIValue *)&(con_rec->protocol), CMPI_uint16);

    if(con_rec->location && con_rec->location[0] != '\0')
        CMSetProperty(inst, "URI", (CMPIValue *)con_rec->location, CMPI_chars);

    // Unused properties
    // 
    //CMSetProperty(inst, "Address",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "AddressOnParent",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ChangeableType",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "ConfigurationName",(CMPIValue *)<value>, CMPI_chars);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Connection",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "Generation",(CMPIValue *)&<value>, CMPI_uint64);
    //CMPIArray *arr = CMNewArray(_BROKER, 1, CMPI_chars, NULL);
    //CMSetArrayElementAt(arr, 0, (CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "HostResource",(CMPIValue *)&arr, CMPI_charsA);
    //CMSetProperty(inst, "MappingBehavior",(CMPIValue *)&<value>, CMPI_uint16);
    //CMSetProperty(inst, "OtherResourceType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Parent",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "PoolID",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "ResourceSubType",(CMPIValue *)<value>, CMPI_chars);
    //CMSetProperty(inst, "Weight",(CMPIValue *)&<value>, CMPI_uint32);

    if(vm_rec)
        xen_vm_record_free(vm_rec);
    return 1;
}
/******************************************************************************
* console_rasd_create_ref
*
* This function creates a CIMObjectPath to represent a reference to a
* Disk RASD object
*
* Returns 1 on Success and 0 on failure.
*******************************************************************************/
CMPIObjectPath *console_rasd_create_ref(
    const CMPIBroker *broker,
    const char *name_space,
    xen_utils_session *session,
    xen_vm_record *vm_rec,
    xen_console con
    )
{
    char inst_id[MAX_INSTANCEID_LEN];
    char *con_uuid = NULL;
    CMPIObjectPath *result_setting = NULL;

    /* find the VBD's uuid */
    if(xen_console_get_uuid(session->xen, &con_uuid, con)) {
        /* create a CIM reference object and set the key properties */
        result_setting = CMNewObjectPath(broker, name_space, "Xen_ConsoleSettingData", NULL);
        if(result_setting) {
            _CMPICreateNewDeviceInstanceID(inst_id, MAX_INSTANCEID_LEN, vm_rec->uuid, con_uuid);
            CMAddKey(result_setting, "InstanceID", (CMPIValue *)inst_id, CMPI_chars);
        }
        free(con_uuid);
    }
    return result_setting;
}
